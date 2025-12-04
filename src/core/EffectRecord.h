#pragma once

#define DX9_GPU_TIMING_INIT(Device)        \
if (DEBUG && TheRenderManager->DXVK)\
{\
    DX9_InitGpuTiming(Device);  \
}


// Start per-effect GPU timing (issues start timestamp)
#define DX9_GPU_TIMING_BEGIN(Device, EffectPtr)                                      \
if (DEBUG && TheRenderManager->DXVK && g_Dx9GpuTimingInit && g_Dx9TicksToMs > 0.0)\
{                  \
    if (!(EffectPtr)->gpuQueryStart)\
    {\
        (Device)->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &(EffectPtr)->gpuQueryStart);\
    }\
    if (!(EffectPtr)->gpuQueryEnd)\
    {\
        (Device)->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &(EffectPtr)->gpuQueryEnd);   \
    }\
    if ((EffectPtr)->gpuQueryStart)\
    {\
        (EffectPtr)->gpuQueryStart->Issue(D3DISSUE_END);                    \
    }\
}

// End per-effect GPU timing (waits for end timestamp, calculates ms)
#define DX9_GPU_TIMING_END(EffectPtr, OutGpuMsVar)                                   \
(OutGpuMsVar) = 0.0f;                                                        \
if (DEBUG && TheRenderManager->DXVK && g_Dx9GpuTimingInit && g_Dx9TicksToMs > 0.0 && (EffectPtr)->gpuQueryStart && (EffectPtr)->gpuQueryEnd)                 \
{                                                                            \
    UINT64 startTicks = 0;                                                   \
    UINT64 endTicks   = 0;                                                   \
    while ((EffectPtr)->gpuQueryEnd->GetData(&endTicks, sizeof(endTicks), D3DGETDATA_FLUSH) == S_FALSE) { }                                \
    while ((EffectPtr)->gpuQueryStart->GetData(&startTicks, sizeof(startTicks), 0) == S_FALSE) { }                                                  \
    if (endTicks > startTicks)\
    {                                             \
        float deltaTicks = (endTicks - startTicks);      \
        (OutGpuMsVar) = (deltaTicks * g_Dx9TicksToMs);     \
    }                                                                        \
}

class EffectRecord : public ShaderProgram
{
public:
    EffectRecord(const char* effectName);
    virtual ~EffectRecord();

    virtual void SetCT();
    virtual void CreateCT(ID3DXBuffer* ShaderSource, ID3DXConstantTable* ConstantTable);

    virtual void UpdateConstants()
    {
    };

    virtual void UpdateSettings()
    {
    };

    virtual void RegisterConstants()
    {
    };

    virtual void RegisterTextures()
    {
    };
    virtual bool ShouldRender() { return true; };
    // reimplement in subclasses to disable render under certain conditions
    virtual bool SwitchEffect();
    virtual void Render(IDirect3DDevice9* Device, IDirect3DSurface9* RenderTarget, IDirect3DSurface9* RenderedSurface,
                        UINT techniqueIndex, bool ClearRenderTarget, IDirect3DSurface9* SourceBuffer);
    void ClearSampler(const char* TextureName, size_t Length);
    void DisposeEffect();
    bool LoadEffect();

    bool IsLoaded();
    bool Enabled;
    float renderTime;
    float constantUpdateTime;
    // --- GPU timing state ---
    struct GpuQuerySlot {
        IDirect3DQuery9* start = nullptr;
        IDirect3DQuery9* end   = nullptr;
        bool             pending = false;
    };

    static const int GPU_QUERY_SLOTS = 4;
    GpuQuerySlot gpuSlots[GPU_QUERY_SLOTS];
    int          gpuSlotIndex = 0;
    float        gpuRenderTimeMs = 0.0f; // last valid GPU time

    void BeginGpuTiming(IDirect3DDevice9* Device);
    void EndGpuTiming();

    // Static frequency / conversion
    static bool gpuTimingInit;
    static double gpuTicksToMs;

    ID3DXEffect* Effect;
    const char* Name;
};
