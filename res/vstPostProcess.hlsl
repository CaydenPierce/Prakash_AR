// Cayden Custom Version

// This is example shader for showcasing how to use video post process filters from
// your own application. In your application, implement your own shader that suits
// your needs.

// Compute shader thread block size
#define BLOCK_SIZE (8)

// Debug texture output modes
#define DEBUG_TEXTURE_OUT_NONE (0)
#define DEBUG_TEXTURE_OUT_RGB (1)
#define DEBUG_TEXTURE_OUT_RG_A (2)

// TODO: We could get this mode in constant buffer to be able to switch runtime
#define DEBUG_TEXTURE_OUT (DEBUG_TEXTURE_OUT_NONE)
//#define DEBUG_TEXTURE_OUT (DEBUG_TEXTURE_OUT_RG_A)

#define VIEW_CONTEXT_L (0)
#define VIEW_CONTEXT_R (1)
#define VIEW_FOCUS_L (2)
#define VIEW_FOCUS_R (3)

#define VIEW_LEFT (0)
#define VIEW_RIGHT (1)

#define LEFT_CHANNEL (0)
#define RIGHT_CHANNEL (1)

// -------------------------------------------------------------------------

// Shader input layout V1 built-in bindings

// Input buffers: t0 = Camera image input
Texture2D<float4> inputTex : register(t0);  // NOTICE! This is sRGBA data bound as RGBA so fetched values are in screen gamma.

// Output buffers: u0 = Camera image output
RWTexture2D<float4> outputTex : register(u0);

// Texture samplers
SamplerState SamplerLinearClamp : register(s0);
SamplerState SamplerLinearWrap : register(s1);

// Varjo generic constants
cbuffer ConstantBuffer : register(b0)
{
    int2 sourceSize;             // Source texture dimensions
    float sourceTime;            // Source texture timestamp
    int viewIndex;               // View to be rendered: 0=LC, 1=RC, 2=LF, 3=RF
    int4 destRect;               // Destination rectangle: x, y, w, h
    float4x4 projection;         // Projection matrix used for the source texture
    float4x4 inverseProjection;  // Inverse projection matrix
    float4x4 view;               // View matrix used for the source texture
    float4x4 inverseView;        // Inverse view matrix
    int4 sourceFocusRect;        // Area of the focus view within the context texture
    int2 sourceContextSize;      // Context texture size
    int2 padding;                // Unused
}


// -------------------------------------------------------------------------

// Shader specific constants
cbuffer ConstantBuffer : register(b1)
{
    // Color grading
    float colorFactor;             // Color grading factor: 0=off, 1=full
    float colorPreserveSaturated;  // Color grading saturated preservation
    float2 _padding_b1_0;          // Padding
    float4 colorValue;             // Color grading value
    float4 colorExp;               // Color grading exponent

    // Noise texture
    float noiseAmount;  // Noise amount: 0=off, 1=full
    float noiseScale;   // Noise scale

    // Blur filter
    float blurScale;     // Blur kernel scale: 0=off, 1=full
    int blurKernelSize;  // Blur kernel size
    float highPassCutoffFreq;  // High pass cutoff frequency
    int filterType;
    float2 _padding_b2_0;          // Padding
}

// Shader specific textures
Texture2D<float4> noiseTexture : register(t1);

// -------------------------------------------------------------------------

static const float Epsilon = 1e-10;

float3 convertRGBtoHCV(in float3 RGB)
{
    float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
    float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
    return float3(H, C, Q.x);
}

// Converts given RGB color to HSV color. RGB = [0..1], HSV = [0..1].
float3 convertRGBtoHSV(in float3 RGB)
{
    float3 HCV = convertRGBtoHCV(RGB);
    float S = HCV.y / (HCV.z + Epsilon);
    return float3(HCV.x, S, HCV.z);
}

// Converts HSV hue channel value to RGB color. H = [0..1].
float3 hueToRGB(in float H)
{
    float R = abs(H * 6.0f - 3.0f) - 1.0f;
    float G = 2.0f - abs(H * 6.0f - 2.0f);
    float B = 2.0f - abs(H * 6.0f - 4.0f);
    return saturate(float3(R, G, B));
}

// Converts given HSV color to RGB color. HSV = [0..1], RGB = [0..1].
float3 hsvToRGB(in float3 HSV)
{
    float3 RGB = hueToRGB(HSV.x);
    return ((RGB - 1.0f) * HSV.y + 1.0f) * HSV.z;
}

float3 homogenize(float4 v) { return v.xyz / v.w; }

float2 pixelToNDC(uint2 pixel, uint2 viewportSize) { return (float2(pixel) + 0.5f) / float2(viewportSize) * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f); }

float2 ndcToUV(float2 ndc) { return (ndc - float2(-1.0f, 1.0f)) / float2(2.0f, -2.0f); }

float3 getViewDir(in float2 ndcCoord, in float4x4 inverseProjection)
{
    float4 dispCoordEnd = float4(ndcCoord, 0.5f, 1.0f);
    float4 viewPosEnd = mul(inverseProjection, dispCoordEnd);
    return normalize(homogenize(viewPosEnd));
}

void calculateKernelParameters(float cpd, out int kernelSize, out float scale)
{
    // Varjo XR-3 specific PPD
    float ppd = 70.0; // Update this value based on precise specifications or empirical findings

    // Convert CPD to spatial frequency in pixels
    float freqInPixels = cpd * 2.0 * ppd; 

    // Inverse relationship between spatial frequency and kernel size
    // This is a simple model; you may need to refine it based on empirical results
    kernelSize = max(3, (int)(ppd / freqInPixels * 15.0)); // Ensure at least a kernel size of 3
    kernelSize = kernelSize - (kernelSize % 2) + 1; // Ensure kernel size is odd

    // Scale based on CPD, adjust as needed
    scale = 1.0 - min(cpd / ppd, 1.0); // Simple linear scale, adjust based on needs
}

// -------------------------------------------------------------------------
#define PI 3.1415926535897932384626433832795

// Compute shader for high pass filtering
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    // Calculate thread coordinates
    const int2 thisThread = dispatchThreadID.xy + int2(destRect.xy);

    // Load source sample
    float4 origColor = inputTex.Load(int3(thisThread.xy, 0)).rgba;
    float4 finalColor = origColor;

    //Invert filter
    if (filterType == 3) { // invert colors
        finalColor.rgb = 1.0 - origColor.rgb;
    }

    //kaleidoscope filter
    if (filterType == 4) { // Assuming 4 is the type for the kaleidoscope effect
        // Convert pixel coordinates to normalized [-1, 1] range centered at texture center
        float2 uv = (float2(thisThread.xy) - 0.5 * sourceSize) / sourceSize.y;

        // Convert to polar coordinates
        float radius = length(uv);
        float angle = atan2(uv.y, uv.x);

        // Define the number of segments
        const int numSegments = 3; // Adjust for different "trippy" effects

        // Reflect angle within a segment and rotate
        float segmentAngle = 2 * PI / numSegments;
        float mirroredAngle = abs(fmod(angle + segmentAngle / 2, segmentAngle) - segmentAngle / 2);
        float newAngle = mirroredAngle * numSegments;

        // Convert back to Cartesian coordinates and scale back to texture coordinates
        float2 newUV = radius * float2(cos(newAngle), sin(newAngle));
        newUV = 0.5 * sourceSize.y * newUV + 0.5 * sourceSize; // Adjusting to texture size

        // Convert to pixel coordinates
        int2 texCoords = int2(newUV.x, newUV.y);

        // Sample the texture
        finalColor = inputTex.Load(int3(texCoords, 0));
    }

    //High and low pass filters
    if (filterType == 1 || filterType == 2 || filterType == 5) {
        //float4 lowPassColor = origColor;
        float4 lowPassColor = float4(0.0, 0.0, 0.0, 0.0);

        // Apply box blur
        if (highPassCutoffFreq > 0.0) {
            int kernelSize;
            float myBlurScale;

            calculateKernelParameters(highPassCutoffFreq, kernelSize, myBlurScale);
            const float2 uv = float2(thisThread) / sourceSize;

            float2 kernelScale = float2(1.0, 1.0) / sourceSize;
            if (viewIndex == VIEW_FOCUS_L || viewIndex == VIEW_FOCUS_R) {
                float2 focusSize = sourceFocusRect.zw - sourceFocusRect.xy;
                kernelScale = float2(1.0, 1.0) / float2(focusSize);
            }

            // Kernel radius and count
            const int kernelD = kernelSize; //blurKernelSize;
            const int kernelR = (kernelD >> 1);
            const int kernelN = (kernelD * kernelD);
            const float2 kernelOffs = (float2(kernelD, kernelD) * 0.5 - 0.5);

            lowPassColor = float4(0.0, 0.0, 0.0, 0.0);
            for (int y = 0; y < kernelD; y++) {
                for (int x = 0; x < kernelD; x++) {
                    const float2 uvOffs = ((float2(x, y) - kernelOffs) * myBlurScale) * kernelScale;
                    lowPassColor += inputTex.SampleLevel(SamplerLinearClamp, uv + uvOffs, 0.0, 0.0);
                }
            }
            lowPassColor /= kernelN;
        }

        if (filterType == 1 || filterType == 5) { //regular high pass filter
            // High pass filter: original - low pass
            finalColor = origColor - lowPassColor;
        }

        if (filterType == 2) { //regular high pass filter
            finalColor = lowPassColor;
        }

        //add normalizer if needed
        float4 normalizer = float4(0.35, 0.35, 0.35, 0.35);
        if (filterType == 1) { //high pass filter
            finalColor = finalColor + normalizer;
        }

        if (filterType == 5) { //SPECIAL trippy high pass filter
            // Optional: Adjust the high pass image to enhance visibility
            finalColor.rgb = abs(finalColor.rgb); // Enhance edges by taking absolute value
            finalColor.rgb = min(finalColor.rgb * 5.0, 1.0); // Scale and clamp values to [0, 1]
        }
    } //end high/low pass logic

    // Write output pixel. Alpha is preserved from the original.
    outputTex[thisThread.xy] = float4(finalColor.rgb, origColor.a);
}
