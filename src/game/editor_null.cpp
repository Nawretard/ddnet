#include <engine/editor.h>

// Priced by the WASM-boundary spike: the editor is 2.38 MiB of the 6.34 MiB of
// compiled client objects, and a client that ships its own interface never
// opens it.
class CNullEditor : public IEditor
{
public:
	void Init() override {}
	void OnUpdate() override {}
	void OnRender() override {}
	void OnActivate() override {}
	void OnWindowResize() override {}
	void OnClose() override {}
	bool HasUnsavedData() const override { return false; }
	bool HandleMapDrop(const char *pFilename, int StorageType) override { return false; }
	bool Load(const char *pFilename, int StorageType) override { return false; }
	bool Save(const char *pFilename) override { return false; }
	void UpdateMentions() override {}
	void ResetMentions() override {}
	void OnIngameMoved() override {}
	void ResetIngameMoved() override {}
};

IEditor *CreateEditor() { return new CNullEditor; }
