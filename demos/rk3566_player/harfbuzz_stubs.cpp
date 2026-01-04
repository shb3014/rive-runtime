// Stub implementations for HarfBuzz functions
// These allow linking but text rendering will not work
extern "C" {
    void rive_hb_font_destroy(void*) {}
    void rive_hb_draw_funcs_destroy(void*) {}
    void* rive_hb_unicode_funcs_get_default() { return nullptr; }
    int rive_hb_unicode_script(unsigned int) { return 0; }
    int rive_hb_unicode_general_category(unsigned int) { return 0; }
    void* rive_hb_font_get_face(void*) { return nullptr; }
    void SBAlgorithmCreate() {}
    void SBAlgorithmRelease(void*) {}
    void* SBAlgorithmCreateParagraph(void*, void*, unsigned long, int) { return nullptr; }
    void SBParagraphRelease(void*) {}
    unsigned long SBParagraphGetLength(void*) { return 0; }
    const void* SBParagraphGetLevelsPtr(void*) { return nullptr; }
    int SBParagraphGetBaseLevel(void*) { return 0; }
}







