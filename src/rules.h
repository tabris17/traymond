#pragma once

#ifdef UNICODE
#define TREGEX std::wregex
#else
#define TREGEX std::regex
#endif

bool loadRules(TRCONTEXT* context);
bool applyRules(TRCONTEXT* context);
bool saveRules(TRCONTEXT* context);
bool clearRules(TRCONTEXT* context);
_Success_(return)
bool matchRule(TRCONTEXT* context, HWND hwnd, bool isMinimizing, _Out_ HIDING_RULE **rulePtr);
INT_PTR showRulesDlg(HWND parent, TRCONTEXT* context);

class RuleEditor final {
private:
    TRCONTEXT* context;
    HWND window = NULL;
    HWND ruleList = NULL;
    HWND nameEdit = NULL;
    HWND textEdit = NULL;
    HWND classEdit = NULL;
    HWND pathEdit = NULL;
    HWND textCheckBox = NULL;
    HWND classCheckBox = NULL;
    HWND pathCheckBox = NULL;
    HWND saveButton = NULL;
    HWND removeButton = NULL;
    HWND dropButton = NULL;
    HWND windowsCombo = NULL;
    HWND onMinimizeRadioBox = NULL;
    HWND onFirstShowRadioBox = NULL;
    HWND onBothRadioBox = NULL;
    HWND neverNotifyRadioBox = NULL;
    HWND alwaysNotifyRadioBox = NULL;
    HWND notifyFirstTimeRadioBox = NULL;
    int ruleId = -1;
    bool isDirty = false;
    bool isBusy = false;
private:
    HIDING_RULE* newRule();
public:
    RuleEditor(TRCONTEXT* context);
    void initialize(HWND hwnd);
    void enable(bool val = true);
    void touch();
    void clean();
    void sync();
    void drop();
    void select();
    void fill();
    void reset(bool init = true);
    bool dirty();
    bool save();
    bool append();
    bool remove();
    bool dispatchMessage(UINT message, WPARAM wParam, LPARAM lParam);
    TRCONTEXT* getContext();
};
