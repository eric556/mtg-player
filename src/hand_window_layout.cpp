#include "hand_window.hpp"
#include <functional>

#define CLAY_IMPLEMENTATION
extern "C" {
    #include <clay.h>
}

// Manual wrapper to avoid MSVC initializer list issues with Clay_String
static Clay_String MakeClayString(const char* s) {
    Clay_String str;
    str.length = (uint32_t)strlen(s);
    str.chars = s;
    return str;
}

static void RenderButton(const char* id, const char* label, bool enabled, std::function<void()>* onClick) {
    Clay_String clayIdStr = MakeClayString(id);
    Clay_ElementId elementId = Clay_GetElementId(clayIdStr);
    
    // Improved hover detection: check if pointer is actually over this button
    bool hovered = Clay_PointerOver(elementId);
    Clay_Color bgColor = enabled ? (hovered ? Clay_Color{90, 90, 100, 255} : Clay_Color{70, 70, 80, 255}) : Clay_Color{45, 45, 45, 255};
    Clay_Color textColor = enabled ? Clay_Color{255, 255, 255, 255} : Clay_Color{100, 100, 100, 255};

    Clay__OpenElementWithId(elementId);
    
    Clay_ElementDeclaration decl = {};
    decl.layout.padding = {15, 15, 8, 8};
    decl.backgroundColor = bgColor;
    Clay__ConfigureOpenElement(decl);

    if (enabled && onClick) {
        Clay_OnHover([](Clay_ElementId elementId, Clay_PointerData pointerData, void* userData) {
            if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
                (*reinterpret_cast<std::function<void()>*>(userData))();
            }
        }, (void*)onClick);
    }

    Clay_TextElementConfig textCfg = {};
    textCfg.textColor = textColor;
    textCfg.fontSize = 14;
    Clay__OpenTextElement(MakeClayString(label), textCfg);
    
    Clay__CloseElement();
}

void HandWindow::createClayLayout() {
    Clay_BeginLayout();

    // Main Container
    Clay__OpenElementWithId(Clay_GetElementId(MakeClayString("MainContainer")));
    Clay_ElementDeclaration mainDecl = {};
    mainDecl.layout.sizing.width = CLAY_SIZING_FIXED(900);
    mainDecl.layout.sizing.height = CLAY_SIZING_FIXED(720);
    mainDecl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    mainDecl.layout.padding = {0, 0, 0, 0}; // No padding so the control bar is at the very top
    Clay__ConfigureOpenElement(mainDecl);

    // Control Row - Fixed at top
    Clay__OpenElementWithId(Clay_GetElementId(MakeClayString("ControlRow")));
    Clay_ElementDeclaration rowDecl = {};
    rowDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    rowDecl.layout.sizing.height = CLAY_SIZING_FIXED(40);
    rowDecl.layout.padding = {10, 10, 5, 5};
    rowDecl.layout.childGap = 10;
    rowDecl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    rowDecl.backgroundColor = {35, 35, 45, 255};
    Clay__ConfigureOpenElement(rowDecl);

    static std::function<void()> drawFn = [&]() { state_.drawCard(); };
    static std::function<void()> shuffleFn = [&]() { state_.shuffleDeck(); };
    static std::function<void()> playFn = [&]() {
        if (selected_hand_idx_ >= 0) {
            float off = static_cast<float>(state_.battlefield.size()) * 115.f;
            float px  = 220.f + std::fmod(off, 820.f);
            float py  = 420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f);
            state_.playCard(selected_hand_idx_, {px, py});
            selected_hand_idx_ = -1;
        }
    };

    RenderButton("DrawButton", "Draw", true, &drawFn);
    RenderButton("ShuffleButton", "Shuffle", true, &shuffleFn);
    
    // Spacer
    Clay__OpenElementWithId(Clay_GetElementId(MakeClayString("Spacer")));
    Clay_ElementDeclaration spacerDecl = {};
    spacerDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    spacerDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
    Clay__ConfigureOpenElement(spacerDecl);
    Clay__CloseElement();

    RenderButton("PlayButton", "▶  Play Card", selected_hand_idx_ >= 0, &playFn);

    Clay__CloseElement(); // ControlRow
    Clay__CloseElement(); // MainContainer

    // Update Clay with very small deltaTime for responsive input
    Clay_RenderCommandArray renderCommands = Clay_EndLayout(0.016f);
    clay_renderer_->render(renderCommands);
}
