/*
* Class that wraps an effect shader, in order to load it/render it/set constants.
*/

// Global/shared DX9 GPU timing state
static bool   g_Dx9GpuTimingInit = false;
static double g_Dx9TicksToMs     = 0.0;

// Helper: one-time initialization of timestamp frequency
static void DX9_InitGpuTiming(IDirect3DDevice9* device)
{
	if (g_Dx9GpuTimingInit)
		return;

	IDirect3DQuery9* freqQuery = nullptr;
	if (FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &freqQuery)) || !freqQuery)
		return;

	freqQuery->Issue(D3DISSUE_END);

	UINT64 freq = 0;
	HRESULT hr;
	int tries = 0;
	const int maxTries = 1000;

	do {
		hr = freqQuery->GetData(&freq, sizeof(freq), D3DGETDATA_FLUSH);
		if (hr == S_OK) break;
		if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG) break;
		::Sleep(0);
	} while (++tries < maxTries);

	freqQuery->Release();

	if (hr == S_OK && freq > 0) {
		g_Dx9TicksToMs     = 1000.0 / static_cast<double>(freq);
		g_Dx9GpuTimingInit = true;
	}
	else {
		g_Dx9TicksToMs     = 0.0;
		g_Dx9GpuTimingInit = false;
	}
}

EffectRecord::EffectRecord(const char* effectName) {

	Name = effectName;

	Effect = NULL;
	Enabled = false;
	renderTime = 0;
	constantUpdateTime = 0;
}

/*Shader Values arrays are freed in the superclass Destructor*/
EffectRecord::~EffectRecord() {
	if (Effect) Effect->Release();
}

/*
 * Unbinds all textures for samplers.
 * Useful when recreating textures on the fly.
 */
void EffectRecord::ClearSampler(const char* TextureName, size_t Length) {
	ShaderTextureValue* Sampler;
	for (UInt32 c = 0; c < TextureShaderValuesCount; c++) {
		Sampler = &TextureShaderValues[c];
		if (!memcmp(Sampler->Name, TextureName, Length) && Sampler->Texture->Texture) {
			Sampler->Texture->Texture = nullptr;
		}
	}
}

/*
 * Unload effects, allowing it to be reloaded from  a blank state.
 */
void EffectRecord::DisposeEffect() {
	if (Effect) Effect->Release();
	Effect = nullptr;

	delete[] FloatShaderValues;
	FloatShaderValues = nullptr;
	FloatShaderValuesCount = 0;

	delete[] TextureShaderValues;
	TextureShaderValues = nullptr;
	TextureShaderValuesCount = 0;

	Enabled = false;
}

/*
 * Compile and Load the Effect shader
 */
bool EffectRecord::LoadEffect() {
	auto timer = TimeLogger();
	bool success = false;

	ID3DXEffect* Effect = NULL;
	ID3DXEffectCompiler* Compiler = NULL;
	ID3DXBuffer* EffectSource = NULL;
	ID3DXBuffer* Errors = NULL;
	ID3DXBuffer* EffectBuffer = NULL;

	char BaseDirectory[MAX_PATH];
	char CacheDirectory[MAX_PATH];
	char EffectSourcePath[MAX_PATH];
	char EffectPreprocessedPath[MAX_PATH];
	char EffectCompiledPath[MAX_PATH];

	// Build filenames.
	strcpy(BaseDirectory, EffectsPath);
	strcpy(CacheDirectory, BaseDirectory);
	strcat(CacheDirectory, "Cache\\");

	strcpy(EffectSourcePath, BaseDirectory);
	strcat(EffectSourcePath, Name);
	strcat(EffectSourcePath, ".fx.hlsl");

	if (!FileExists(EffectSourcePath)) {
		return success;
	}

	strcpy(EffectPreprocessedPath, CacheDirectory);
	strcat(EffectPreprocessedPath, Name);
	strcat(EffectPreprocessedPath, ".fx.hlsl");

	strcpy(EffectCompiledPath, CacheDirectory);
	strcat(EffectCompiledPath, Name);
	strcat(EffectCompiledPath, ".fx");

	HRESULT prepass = D3DXPreprocessShaderFromFileA(EffectSourcePath, NULL, NULL, &EffectSource, &Errors);
	if (prepass == D3D_OK) {
		bool Compile = !CheckPreprocessResult(EffectPreprocessedPath, EffectSource);
		bool CompiledExists = FileExists(EffectCompiledPath);

		if (!Compile && CompiledExists) {
			HRESULT loaded = D3DXCreateEffectFromFileA(TheRenderManager->device, EffectCompiledPath, NULL, NULL, D3DXFX_LARGEADDRESSAWARE, NULL, &Effect, &Errors);
			if (FAILED(loaded)) {
				ReportError(loaded);
				if (Errors) Logger::Log((char*)Errors->GetBufferPointer());

				if (Effect) { Effect->Release(); Effect = NULL; }
				if (Errors) { Errors->Release(); Errors = NULL; }
			}

			if (Errors) Logger::Log((char*)Errors->GetBufferPointer());
			
			success = true;
		}

		if (Compile || !CompiledExists || !Effect) {
			// compile if option was enabled or compiled version not found
			HRESULT compiled = D3DXCreateEffectCompiler(
				(const char*)EffectSource->GetBufferPointer(), 
				EffectSource->GetBufferSize(), 
				NULL, 
				NULL, 
				NULL, 
				&Compiler, 
				&Errors
			);
			if (FAILED(compiled)) {
				ReportError(compiled);
				if (Errors) Logger::Log((char*)Errors->GetBufferPointer());
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				Enabled = false;
				return success;
			}
			if (Errors) Logger::Log((char*)Errors->GetBufferPointer());

			compiled = Compiler->CompileEffect(NULL, &EffectBuffer, &Errors);
			if (FAILED(compiled)) {
				ReportError(compiled);
				if (Errors) Logger::Log((char*)Errors->GetBufferPointer());
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				Enabled = false;
				return success;
			}
			if (Errors) Logger::Log((char*)Errors->GetBufferPointer());

			if (EffectBuffer) {
				std::ofstream FileBinary(EffectCompiledPath, std::ios::out | std::ios::binary);
				FileBinary.write((const char*)EffectBuffer->GetBufferPointer(), EffectBuffer->GetBufferSize());
				FileBinary.flush();
				FileBinary.close();

				std::ofstream FilePreprocess(EffectPreprocessedPath, std::ios::out | std::ios::binary);
				FilePreprocess.write((const char*)EffectSource->GetBufferPointer(), EffectSource->GetBufferSize());
				FilePreprocess.flush();
				FilePreprocess.close();

				Logger::Log("Effect compiled: %s", EffectPreprocessedPath);
			}

			HRESULT loaded = D3DXCreateEffectFromFileA(TheRenderManager->device, EffectCompiledPath, NULL, NULL, D3DXFX_LARGEADDRESSAWARE, NULL, &Effect, &Errors);
			if (FAILED(loaded)) {
				ReportError(loaded);
				if (Errors) Logger::Log((char*)Errors->GetBufferPointer());
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				if (EffectBuffer) EffectBuffer->Release();
				Enabled = false;
				return success;
			}
			if (Errors) Logger::Log((char*)Errors->GetBufferPointer());

			success = true;
		}
	}
	else {
		ReportError(prepass);
		if (Errors) Logger::Log((char*)Errors->GetBufferPointer());
	}

	if (Effect) {
		this->Effect = Effect;
		CreateCT(EffectSource, NULL); //Create the object which will associate a register index to a float pointer for constants updates;
		Logger::Log("Effect loaded: %s", EffectCompiledPath);
	}

	// set enabled status of effect based on success and setting
	Enabled = Effect != nullptr && TheSettingManager->GetMenuShaderEnabled(Name);

	if (Compiler) Compiler->Release();
	if (Errors) Errors->Release();
	if (EffectBuffer) EffectBuffer->Release();
	if (EffectSource) EffectSource->Release();

	timer.LogTime("EffectRecord::LoadEffect");

	return success;
}


/**
* Loads an effect shader by name (The post process effects stored in the Effects folder)
* @param Name the name for the effect
* @returns an EffectRecord describing the effect shader.
*/
bool EffectRecord::IsLoaded() {
	return Effect != nullptr;
}


/**
Creates the Constant Table for the Effect Record.
*/
void EffectRecord::CreateCT(ID3DXBuffer* ShaderSource, ID3DXConstantTable* ConstantTable) {
	auto timer = TimeLogger();

	D3DXEFFECT_DESC ConstantTableDesc;
	D3DXPARAMETER_DESC ConstantDesc;
	D3DXHANDLE Handle;
	UINT ConstantCount = 1;
	UInt32 FloatIndex = 0;
	UInt32 TextureIndex = 0;

	Effect->GetDesc(&ConstantTableDesc);
	for (UINT c = 0; c < ConstantTableDesc.Parameters; c++) {
		Handle = Effect->GetParameter(NULL, c);
		Effect->GetParameterDesc(Handle, &ConstantDesc);
		if (memcmp(ConstantDesc.Name, "TESR_", 5)) continue;
		if ((ConstantDesc.Class == D3DXPC_VECTOR || ConstantDesc.Class == D3DXPC_MATRIX_ROWS)) FloatShaderValuesCount += 1;
		if (ConstantDesc.Class == D3DXPC_OBJECT && ConstantDesc.Type >= D3DXPT_SAMPLER && ConstantDesc.Type <= D3DXPT_SAMPLERCUBE) TextureShaderValuesCount += 1;
	}
	FloatShaderValues = new ShaderFloatValue[FloatShaderValuesCount];
	TextureShaderValues = new ShaderTextureValue[TextureShaderValuesCount];

	//Logger::Debug("CreateCT: Effect has %i constants", ConstantTableDesc.Parameters);

	for (UINT c = 0; c < ConstantTableDesc.Parameters; c++) {
		Handle = Effect->GetParameter(NULL, c);
		Effect->GetParameterDesc(Handle, &ConstantDesc);
		if (memcmp(ConstantDesc.Name, "TESR_", 5)) continue; // Only treat constants named with TESR prefix

		switch (ConstantDesc.Class) {
		case D3DXPC_VECTOR:
			FloatShaderValues[FloatIndex].Name = ConstantDesc.Name;
			FloatShaderValues[FloatIndex].Type = (D3DXPARAMETER_TYPE)D3DXPC_VECTOR;
			FloatShaderValues[FloatIndex].RegisterIndex = (UInt32)Handle;
			FloatShaderValues[FloatIndex].RegisterCount = ConstantDesc.Elements;
			FloatIndex++;
			break;
		case D3DXPC_MATRIX_ROWS:
			FloatShaderValues[FloatIndex].Name = ConstantDesc.Name;
			FloatShaderValues[FloatIndex].Type = (D3DXPARAMETER_TYPE)D3DXPC_MATRIX_ROWS;
			FloatShaderValues[FloatIndex].RegisterIndex = (UInt32)Handle;
			FloatShaderValues[FloatIndex].RegisterCount = ConstantDesc.Elements;
			FloatIndex++;
			break;
		case D3DXPC_OBJECT:
			if (ConstantDesc.Class == D3DXPC_OBJECT && ConstantDesc.Type >= D3DXPT_SAMPLER && ConstantDesc.Type <= D3DXPT_SAMPLERCUBE) {
				TextureShaderValues[TextureIndex].Name = ConstantDesc.Name;
				TextureShaderValues[TextureIndex].Type = TextureRecord::GetTextureType(ConstantDesc.Type);
				TextureShaderValues[TextureIndex].RegisterIndex = TextureIndex;
				TextureShaderValues[TextureIndex].RegisterCount = 1;
				TextureShaderValues[TextureIndex].GetSamplerStateString(ShaderSource, TextureIndex);
				TextureShaderValues[TextureIndex].GetTextureRecord();

				TextureIndex++;
			}
			break;
		}
	}

	timer.LogTime("EffectRecord::ConstantTableDesc");
}

/*
*Sets the Effect Shader constants table and texture registers.
*/
void EffectRecord::SetCT() {
	if (!Enabled || Effect == nullptr) return;

	ShaderTextureValue* Sampler;
	for (UInt32 c = 0; c < TextureShaderValuesCount; c++) {
		Sampler = &TextureShaderValues[c];
		if (!Sampler->Texture->Texture) {
			Sampler->Texture->BindTexture(Sampler->Name);

			if (Sampler->Texture->Texture) Logger::Log("%s : Texture %s Succesfully bound", Name, Sampler->Name);
			//else Logger::Log("[ERROR] : Could not bind texture %s for effect %s", Sampler->Name, Name);
		}

		if (Sampler->Texture->Texture != nullptr) {
			TheRenderManager->device->SetTexture(Sampler->RegisterIndex, Sampler->Texture->Texture);
			for (int i = 1; i < SamplerStatesMax; i++) {
				TheRenderManager->SetSamplerState(Sampler->RegisterIndex, (D3DSAMPLERSTATETYPE)i, Sampler->Texture->SamplerStates[i]);
			}
		}
	}

	ShaderFloatValue* Constant;
	for (UInt32 c = 0; c < FloatShaderValuesCount; c++) {
		Constant = &FloatShaderValues[c];
		if (Constant->Value == nullptr)  Constant->GetValueFromConstantTable();
		if (Constant->Value == nullptr) {
			Logger::Log("[Error] %s : Couldn't get value for Constant %s", Name, Constant->Name);
			continue;
		}

		if (Constant->Type == (D3DXPARAMETER_TYPE)D3DXPC_VECTOR) {
			if (Constant->RegisterCount > 1)
				Effect->SetVectorArray((D3DXHANDLE)Constant->RegisterIndex, Constant->Value, Constant->RegisterCount);
			else
				Effect->SetVector((D3DXHANDLE)Constant->RegisterIndex, Constant->Value);
		}
		else
			if (Constant->RegisterCount > 1)
				Effect->SetMatrixArray((D3DXHANDLE)Constant->RegisterIndex, (D3DXMATRIX*)Constant->Value, Constant->RegisterCount);
			else
				Effect->SetMatrix((D3DXHANDLE)Constant->RegisterIndex, (D3DXMATRIX*)Constant->Value);
	}
}

/*
 * Enable or Disable Effect, with loading if not loaded
 */
bool EffectRecord::SwitchEffect() {
	bool change = true;
	if (!IsLoaded() && !Enabled) {
		Logger::Log("Effect %s is not loaded", Name);
		DisposeEffect();
		change = LoadEffect();
	}
	if (change) {
		Enabled = !Enabled;
	}
	else {
		char Message[256] = "Error: Couldn't enable effect ";
		strcat(Message, Name);

		InterfaceManager->ShowMessage(Message);
		Logger::Log(Message);
	}
	return Enabled;
}

void EffectRecord::BeginGpuTiming(IDirect3DDevice9* Device)
{
    if (!DEBUG || !TheRenderManager->DXVK)
        return;
    if (!g_Dx9GpuTimingInit || g_Dx9TicksToMs <= 0.0)
        return;

    // 1) Try to resolve the *previous* slot (non-blocking-ish)
    GpuQuerySlot& prevSlot = gpuSlots[gpuSlotIndex];
    if (prevSlot.pending && prevSlot.start && prevSlot.end)
    {
        UINT64 startTicks = 0;
        UINT64 endTicks   = 0;

        HRESULT hrStart = prevSlot.start->GetData(&startTicks, sizeof(startTicks), 0);
        HRESULT hrEnd   = prevSlot.end->GetData(&endTicks,   sizeof(endTicks),
                                                D3DGETDATA_FLUSH);

        if (hrStart == S_OK && hrEnd == S_OK && endTicks > startTicks)
        {
            double deltaTicks = double(endTicks - startTicks);
            gpuRenderTimeMs   = float(deltaTicks * g_Dx9TicksToMs);
        }

        // Whether successful or not, this slot is no longer pending once GPU reached it
        if (hrStart == S_OK || hrStart == D3DERR_DEVICELOST || hrStart == D3DERR_DEVICEHUNG ||
            hrEnd   == S_OK || hrEnd   == D3DERR_DEVICELOST   || hrEnd   == D3DERR_DEVICEHUNG)
        {
            prevSlot.pending = false;
        }
    }

    // 2) Advance to a new slot for this frame
    gpuSlotIndex = (gpuSlotIndex + 1) % GPU_QUERY_SLOTS;
    GpuQuerySlot& curSlot = gpuSlots[gpuSlotIndex];

    if (!curSlot.start)
        Device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &curSlot.start);
    if (!curSlot.end)
        Device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &curSlot.end);

    if (curSlot.start && curSlot.end)
    {
        curSlot.start->Issue(D3DISSUE_END); // "start" timestamp
        curSlot.pending = false;            // will be set true in EndGpuTiming
    }
}

void EffectRecord::EndGpuTiming()
{
    if (!DEBUG || !TheRenderManager->DXVK)
        return;
    if (!g_Dx9GpuTimingInit || g_Dx9TicksToMs <= 0.0)
        return;

    GpuQuerySlot& curSlot = gpuSlots[gpuSlotIndex];
    if (curSlot.end && curSlot.start)
    {
        curSlot.end->Issue(D3DISSUE_END); // "end" timestamp
        curSlot.pending = true;           // will be resolved on next BeginGpuTiming()
    }
}

/**
* Renders the given effect shader.
*/
void EffectRecord::Render(IDirect3DDevice9* Device,
                          IDirect3DSurface9* RenderTarget,
                          IDirect3DSurface9* RenderedSurface,
                          UINT techniqueIndex,
                          bool ClearRenderTarget,
                          IDirect3DSurface9* SourceBuffer)
{
    if (!Enabled || Effect == nullptr || !ShouldRender()) {
        renderTime = 0.0f;
        return;
    }

    auto timer = TimeLogger();

    // Init global GPU timing once (if DXVK+DEBUG)
    if (DEBUG && TheRenderManager->DXVK)
        DX9_InitGpuTiming(Device);

    // Begin GPU timing (reads previous slot’s result + starts new one)
    BeginGpuTiming(Device);

    if (SourceBuffer)
        Device->StretchRect(RenderTarget, NULL, SourceBuffer, NULL, D3DTEXF_LINEAR);

    try {
        D3DXHANDLE technique = Effect->GetTechnique(techniqueIndex);
        Effect->SetTechnique(technique);
        SetCT(); // update constants

        UINT Passes;
        Effect->Begin(&Passes, NULL);
        for (UINT p = 0; p < Passes; p++) {
            if (ClearRenderTarget)
                Device->Clear(0L, NULL, D3DCLEAR_TARGET,
                              D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0L);

            Effect->BeginPass(p);
            Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            Effect->EndPass();

            if (RenderedSurface)
                Device->StretchRect(RenderTarget, NULL, RenderedSurface, NULL, D3DTEXF_LINEAR);
        }
        Effect->End();
    }
    catch (const std::exception& e) {
        Logger::Log("Error during rendering of effect %s: %s", Name, e.what());
    }

    // End GPU timing (marks current slot pending)
    EndGpuTiming();

    std::string name = "EffectRecord::Render " + std::string(Name);
    float cpuMs = timer.LogTime(name.c_str());

    if (DEBUG && TheRenderManager->DXVK && g_Dx9GpuTimingInit && g_Dx9TicksToMs > 0.0) {
        // Prefer last known GPU time, but don’t block for it
        renderTime = gpuRenderTimeMs;
        Logger::Log("%s GPU: %.3f ms (CPU: %.3f ms)", Name, gpuRenderTimeMs, cpuMs);
    } else {
        renderTime = cpuMs;
    }
}

