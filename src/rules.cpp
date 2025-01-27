#include <windows.h>
#include <windowsx.h>
#include <regex>

#include "resource.h"
#include "traymond.h"
#include "winevent.h"
#include "rules.h"

static bool serializeRules(HIDING_RULES* rules, LPBYTE data, _Inout_ size_t* size)
{
    size_t realSize = 0;

    for (HIDING_RULE* rule : *rules) {
        realSize += rule->size;
    }

    if (data) {
        if (realSize > *size) {
            return false;
        }
        auto offset = data;
        for (HIDING_RULE* rule : *rules) {
            memcpy(offset, rule, rule->size);
            offset += rule->size;
        }
    }
    else {
        *size = realSize;
    }
    return true;
}

static bool unserializeRules(HIDING_RULES* rules, LPBYTE data, size_t size)
{
    HIDING_RULE* rule = NULL;

    for (size_t i = 0; i < size; i += rule->size) {
        rule = (HIDING_RULE*)(data + i);
        if (i + rule->size > size) {
            return false;
        }
        HIDING_RULE* ruleDest = (HIDING_RULE*) new BYTE[rule->size];
        memcpy(ruleDest, rule, rule->size);
        rules->push_back(ruleDest);
    }
    return true;
}

bool saveRules(TRCONTEXT* context)
{
    bool result = false;
    HKEY regKey = NULL;

    if (ERROR_SUCCESS == RegCreateKey(HKEY_CURRENT_USER, REG_KEY_SOFTWARE, &regKey)) {
        size_t rulesSize = 0;
        if (serializeRules(&context->hidingRules, NULL, &rulesSize)) {
            BYTE* rules = new BYTE[rulesSize];
            if (serializeRules(&context->hidingRules, rules, &rulesSize)) {
                result = ERROR_SUCCESS == RegSetValueEx(regKey, _T("Rules"), 0, REG_BINARY, rules, rulesSize);
            }
            delete[] rules;
        }
        RegCloseKey(regKey);
    }
    return result;
}

bool loadRules(TRCONTEXT* context)
{
    bool result = false;
    DWORD dataSize = 0;

    clearRules(context);

    if (ERROR_SUCCESS == RegGetValue(HKEY_CURRENT_USER, REG_KEY_SOFTWARE, _T("Rules"), RRF_RT_REG_BINARY, NULL, NULL, &dataSize)) {
        BYTE* data = new BYTE[dataSize];
        if (ERROR_SUCCESS == RegGetValue(HKEY_CURRENT_USER, REG_KEY_SOFTWARE, _T("Rules"), RRF_RT_REG_BINARY, NULL, data, &dataSize)) {
            result = unserializeRules(&context->hidingRules, data, dataSize);
        }
        delete[] data;
    }
    return result;
}

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto context = reinterpret_cast<TRCONTEXT*>(lParam);
    HIDING_RULE *rule = nullptr;
    if (IsWindowVisible(hwnd) && matchRule(context, hwnd, false , &rule)) {
        minimizeWindow(context, hwnd);
        if (RULE_IS_ALWAYS_NOTIFY(rule->flag) || 
            RULE_IS_NOTIFY_FIRST_TIME(rule->flag)) {

            notifyHidingWindow(context, hwnd);
        }
    }
    return TRUE;
}

bool applyRules(TRCONTEXT* context)
{
    if (!context->autoHiding) {
        return false;
    }

    return (bool)EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(context));
}

bool clearRules(TRCONTEXT* context)
{
    if (context->hidingRules.empty()) { 
        return false; 
    }

    for (HIDING_RULE* rule : context->hidingRules) {
        delete[] (BYTE*)rule;
    }

    context->hidingRules.clear();
    return true;
}

inline static bool compareText(PTCHAR text, PTCHAR pattern, bool isRegex)
{
    if (isRegex) {
        try {
            TREGEX expr(pattern);
            return std::regex_match(text, expr);
        }
        catch (const std::regex_error&) {
            return false;
        }
    }
    else {
        return _tcscmp(text, pattern) == 0;
    }
}

_Success_(return)
bool matchRule(TRCONTEXT* context, HWND hwnd, bool isMinimizing, _Out_ HIDING_RULE **rulePtr)
{
    TCHAR windowText[MAX_WINDOW_TEXT] {};
    TCHAR className[MAX_CLASS_NAME] {};
    TCHAR exeFileName[MAX_PATH] {};
    if (!GetWindowText(hwnd, windowText, MAX_WINDOW_TEXT) ||
        !GetClassName(hwnd, className, MAX_CLASS_NAME) || 
        !GetWindowExeFileName(hwnd, exeFileName, MAX_PATH)) {

        return false;
    }
    for (auto rule : context->hidingRules) {
        if (isMinimizing) {
            if (!(rule->flag & RULE_ON_MINIMIZE)) {
                continue;
            }
        }
        else {
            if (rule->flag & RULE_AUTO_OFF) {
                continue;
            }
        }
        auto text = rule->ruleData;
        text += _tcsclen(text) + 1;
        if (!compareText(windowText, text, rule->flag & RULE_REGEX_WINDOW_TEXT)) {
            continue;
        }
        text += _tcsclen(text) + 1;
        if (!compareText(className, text, rule->flag & RULE_REGEX_WINDOW_CLASS)) {
            continue;
        }
        text += _tcsclen(text) + 1;
        if (!compareText(exeFileName, text, rule->flag & RULE_REGEX_EXE_FILENAME)) {
            continue;
        }
        *rulePtr = rule;
        return true;
    }
    return false;
}

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
    RuleEditor* editor;
    if (WM_INITDIALOG == message) {
        editor = reinterpret_cast<RuleEditor*>(lParam);
        editor->initialize(hwndDlg);
        return TRUE;
    }
    editor = reinterpret_cast<RuleEditor*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
    return editor->dispatchMessage(message, wParam, lParam);
}

INT_PTR showRulesDlg(HWND parent, TRCONTEXT* context)
{
    static bool dialogOpened = false;
    if (dialogOpened) {
        return FALSE;
    }
    dialogOpened = true;
    auto editor = RuleEditor(context);
    auto result = DialogBoxParam(
        i18n.lang(context->instance),
        MAKEINTRESOURCE(IDD_RULES),
        parent,
        (DLGPROC)DialogProc,
        (LPARAM)&editor
    );
    dialogOpened = false;
    return result;
}

inline static bool testRegex(HWND hwnd, PTCHAR pattern)
{
    TCHAR errMsg[MAX_MSG]{};
    try {
        TREGEX expr(pattern);
    }
    catch (const std::regex_error&) {
        _stprintf_s(errMsg, i18n[IDS_INVALID_REGEX], pattern);
        MessageBox(hwnd, errMsg, APP_NAME, MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

HIDING_RULE* RuleEditor::newRule()
{
    size_t size = sizeof(HIDING_RULE);
    TCHAR ruleName[MAX_RULE_NAME], 
          windowText[MAX_WINDOW_TEXT], 
          windowClassName[MAX_CLASS_NAME],
          exeFileName[MAX_PATH];
    
    auto ruleNameSize = Edit_GetText(nameEdit, ruleName, MAX_RULE_NAME) + 1,
         windowTextSize = Edit_GetText(textEdit, windowText, MAX_WINDOW_TEXT) + 1,
         windowClassNameSize = Edit_GetText(classEdit, windowClassName, MAX_CLASS_NAME) + 1,
         exeFileNameSize = Edit_GetText(pathEdit, exeFileName, MAX_PATH) + 1;
    bool isWindowTextRegex = Button_GetCheck(textCheckBox) == BST_CHECKED,
        isWindowClassNameRegex = Button_GetCheck(classCheckBox) == BST_CHECKED,
        isExeFileNameRegex = Button_GetCheck(pathCheckBox) == BST_CHECKED,
        isAlwaysNotify = Button_GetCheck(alwaysNotifyRadioBox) == BST_CHECKED,
        isNotifyFirstTime = Button_GetCheck(notifyFirstTimeRadioBox) == BST_CHECKED,
        isOnMinimize = Button_GetCheck(onMinimizeRadioBox) == BST_CHECKED,
        isOnBoth = Button_GetCheck(onBothRadioBox) == BST_CHECKED;
    
    if (ruleNameSize == 1 || windowTextSize == 1 || windowClassNameSize == 1 || exeFileNameSize == 1) {
        MessageBox(window, i18n[IDS_RULE_INFO_REQUIRED], APP_NAME, MB_OK | MB_ICONWARNING);
        return NULL;
    }

    if (isWindowTextRegex && !testRegex(window, windowText)) {
        return NULL;
    }
    if (isWindowClassNameRegex && !testRegex(window, windowClassName)) {
        return NULL;
    }
    if (isExeFileNameRegex && !testRegex(window, exeFileName)) {
        return NULL;
    }
    
    size += (ruleNameSize + windowTextSize + windowClassNameSize + exeFileNameSize) * sizeof(TCHAR);
    HIDING_RULE* rule = (HIDING_RULE*)new BYTE[size];
    rule->size = size;
    rule->flag = 0;
    if (isWindowTextRegex) rule->flag |= RULE_REGEX_WINDOW_TEXT;
    if (isWindowClassNameRegex) rule->flag |= RULE_REGEX_WINDOW_CLASS;
    if (isExeFileNameRegex) rule->flag |= RULE_REGEX_EXE_FILENAME;
    if (isOnMinimize) {
        rule->flag |= (RULE_ON_MINIMIZE | RULE_AUTO_OFF);
    }
    else if (isOnBoth) {
        rule->flag |= RULE_ON_MINIMIZE;
    }
    if (isAlwaysNotify) {
        rule->flag |= RULE_ALWAYS_NOTIFY;
    }
    else if (isNotifyFirstTime) {
        rule->flag |= RULE_NOTIFY_FIRST_TIME;
    }
    PTCHAR ruleDataOffset = rule->ruleData;
    memcpy(ruleDataOffset, ruleName, ruleNameSize * sizeof(TCHAR));
    ruleDataOffset += ruleNameSize;
    memcpy(ruleDataOffset, windowText, windowTextSize * sizeof(TCHAR));
    ruleDataOffset += windowTextSize;
    memcpy(ruleDataOffset, windowClassName, windowClassNameSize * sizeof(TCHAR));
    ruleDataOffset += windowClassNameSize;
    memcpy(ruleDataOffset, exeFileName, exeFileNameSize * sizeof(TCHAR));
    return rule;
}

RuleEditor::RuleEditor(TRCONTEXT* context)
{
    this->context = context;
}

TRCONTEXT* RuleEditor::getContext()
{
    return context;
}

void RuleEditor::initialize(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG>(this));
    window = hwnd;
    ruleList = GetDlgItem(hwnd, IDC_LIST_RULES);
    nameEdit = GetDlgItem(hwnd, IDC_EDIT_NAME);
    textEdit = GetDlgItem(hwnd, IDC_EDIT_TEXT);
    classEdit = GetDlgItem(hwnd, IDC_EDIT_CLASS);
    pathEdit = GetDlgItem(hwnd, IDC_EDIT_PATH);
    textCheckBox = GetDlgItem(hwnd, IDC_CHECK_REGEX_TEXT);
    classCheckBox = GetDlgItem(hwnd, IDC_CHECK_REGEX_CLASS);
    pathCheckBox = GetDlgItem(hwnd, IDC_CHECK_REGEX_PATH);
    saveButton = GetDlgItem(hwnd, IDC_SAVE);
    removeButton = GetDlgItem(hwnd, IDC_REMOVE);
    dropButton = GetDlgItem(hwnd, IDABORT);
    windowsCombo = GetDlgItem(hwnd, IDC_COMBO_WINDOWS);
    onMinimizeRadioBox = GetDlgItem(hwnd, IDC_RADIO_ON_MINIMIZE);
    onFirstShowRadioBox = GetDlgItem(hwnd, IDC_RADIO_ON_FIRST_SHOW);
    onBothRadioBox = GetDlgItem(hwnd, IDC_RADIO_ON_BOTH);
    neverNotifyRadioBox = GetDlgItem(hwnd, IDC_RADIO_NEVER_NOTIFY);
    alwaysNotifyRadioBox = GetDlgItem(hwnd, IDC_RADIO_ALWAYS_NOTIFY);
    notifyFirstTimeRadioBox = GetDlgItem(hwnd, IDC_RADIO_NOTIFY_FIRST_TIME);
    Edit_LimitText(nameEdit, MAX_RULE_NAME);
    Edit_LimitText(textEdit, MAX_WINDOW_TEXT);
    Edit_LimitText(classEdit, MAX_CLASS_NAME);
    Edit_LimitText(pathEdit, MAX_PATH);
    sync();
}

#pragma warning(push)
#pragma warning(disable:4100)
bool RuleEditor::dispatchMessage(UINT message, WPARAM wParam, LPARAM lParam)
#pragma warning(pop)
{
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDHELP:
            ShellExecute(window, _T("open"), HELP_URL, NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDABORT:
            drop();
            return TRUE;
        case IDCANCEL:
            return EndDialog(window, wParam);
        case IDC_NEW:
            return append();
        case IDC_REMOVE:
            return remove();
        case IDC_SAVE:
            return save();
        case IDC_LIST_RULES:
            switch (HIWORD(wParam)) {
            case LBN_SELCANCEL:
            case LBN_SELCHANGE:
                select();
                break;
            }
            break;
        case IDC_EDIT_NAME:
        case IDC_EDIT_TEXT:
        case IDC_EDIT_CLASS:
        case IDC_EDIT_PATH:
            if (HIWORD(wParam) == EN_CHANGE) {
                touch();
            }
            break;
        case IDC_RADIO_ON_MINIMIZE:
        case IDC_RADIO_ON_FIRST_SHOW:
        case IDC_RADIO_ON_BOTH:
        case IDC_RADIO_NEVER_NOTIFY:
        case IDC_RADIO_ALWAYS_NOTIFY:
        case IDC_RADIO_NOTIFY_FIRST_TIME:
        case IDC_CHECK_REGEX_CLASS:
        case IDC_CHECK_REGEX_PATH:
        case IDC_CHECK_REGEX_TEXT:
            touch();
            break;
        case IDC_COMBO_WINDOWS:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                fill();
            }
            break;
        }
        break;
    }
    return FALSE;
}

bool RuleEditor::append()
{
    if (dirty()) {
        switch (MessageBox(window, i18n[IDS_UNSAVED], APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION)) {
        case IDYES:
            save();
            break;
        case IDNO:
            drop();
            break;
        case IDCANCEL: 
            return FALSE;
        }
    }
    sync();
    TCHAR ruleName[MAX_RULE_NAME];
    _stprintf_s(ruleName, _T("* %s"), i18n[IDS_NEW_RULE]);
    auto index = ListBox_AddString(ruleList, ruleName);
    ListBox_SetCurSel(ruleList, index);
    reset(true);
    select();
    enable();
    Edit_SetText(nameEdit, i18n[IDS_NEW_RULE]);
    return TRUE;
}

bool RuleEditor::remove()
{
    auto index = ListBox_GetCurSel(ruleList);
    if (index < 0) {
        return false;
    }
    if ((size_t)index < context->hidingRules.size()) {
        auto rule = context->hidingRules[index];
        delete[] (BYTE*)rule;
        VECTOR_ERASE(context->hidingRules, index);
    }
    ListBox_DeleteString(ruleList, index);
    ListBox_SetCurSel(ruleList, -1);
    select();
    enable(false);
    reset(false);
    clean();
    return saveRules(context);
}

void RuleEditor::reset(bool init)
{
    Button_SetCheck(textCheckBox, BST_UNCHECKED);
    Button_SetCheck(classCheckBox, BST_UNCHECKED);
    Button_SetCheck(pathCheckBox, BST_UNCHECKED);
    Button_SetCheck(onMinimizeRadioBox, BST_UNCHECKED);
    Button_SetCheck(onBothRadioBox, BST_UNCHECKED);
    Button_SetCheck(alwaysNotifyRadioBox, BST_UNCHECKED);
    Button_SetCheck(notifyFirstTimeRadioBox, BST_UNCHECKED);
    Edit_SetText(nameEdit, NULL);
    Edit_SetText(textEdit, NULL);
    Edit_SetText(classEdit, NULL);
    Edit_SetText(pathEdit, NULL);
    if (init) {
        Button_SetCheck(onFirstShowRadioBox, BST_CHECKED);
        Button_SetCheck(neverNotifyRadioBox, BST_CHECKED);
    }
    else {
        Button_SetCheck(onFirstShowRadioBox, BST_UNCHECKED);
        Button_SetCheck(neverNotifyRadioBox, BST_UNCHECKED);
    }
}

void RuleEditor::enable(bool val)
{
    Edit_Enable(nameEdit, val);
    Edit_Enable(textEdit, val);
    Edit_Enable(classEdit, val);
    Edit_Enable(pathEdit, val);
    Button_Enable(textCheckBox, val);
    Button_Enable(classCheckBox, val);
    Button_Enable(pathCheckBox, val);
    ComboBox_Enable(windowsCombo, val);
    Button_Enable(onMinimizeRadioBox, val);
    Button_Enable(onFirstShowRadioBox, val);
    Button_Enable(onBothRadioBox, val);
    Button_Enable(neverNotifyRadioBox, val);
    Button_Enable(alwaysNotifyRadioBox, val);
    Button_Enable(notifyFirstTimeRadioBox, val);
}

void RuleEditor::touch()
{
    if (!isDirty && !isBusy) {
        isDirty = true;
        Button_Enable(saveButton, TRUE);
        Button_Enable(dropButton, TRUE);
        ListBox_Enable(ruleList, FALSE);
    }
}

void RuleEditor::clean()
{
    if (isDirty) {
        isDirty = false;
        Button_Enable(saveButton, FALSE);
        Button_Enable(dropButton, FALSE);
        ListBox_Enable(ruleList, TRUE);
    }
}

void RuleEditor::sync()
{
    ListBox_ResetContent(ruleList);
    for (HIDING_RULE* rule : context->hidingRules) {
        ListBox_AddString(ruleList, rule->ruleData);
    }

    ComboBox_ResetContent(windowsCombo);
    TCHAR windowText[MAX_WINDOW_TEXT]{};
    for (int i = 0; i < context->iconIndex; i++) {
        auto hiddenWindow = context->icons[i].window;
        if (GetWindowText(hiddenWindow, windowText, MAX_WINDOW_TEXT) == 0) {
            continue;
        }
        ComboBox_SetItemData(windowsCombo, ComboBox_AddString(windowsCombo, windowText), i);
    }
}

void RuleEditor::drop()
{
    auto index = ListBox_GetCurSel(ruleList);
    if ((size_t)index >= context->hidingRules.size()) {
        ListBox_DeleteString(ruleList, index);
    }
    ListBox_SetCurSel(ruleList, -1);
    select();
    enable(false);
    reset(false);
    clean();
}

void RuleEditor::select()
{
    isBusy = true;
    auto index = ListBox_GetCurSel(ruleList);
    Button_Enable(removeButton, index > -1 ? TRUE : FALSE);
    if (index > -1 && (size_t)index < context->hidingRules.size()) {
        enable();
        auto rule = context->hidingRules[index];
        auto text = rule->ruleData;
        Edit_SetText(nameEdit, text);
        text += _tcsclen(text) + 1;
        Edit_SetText(textEdit, text);
        text += _tcsclen(text) + 1;
        Edit_SetText(classEdit, text);
        text += _tcsclen(text) + 1;
        Edit_SetText(pathEdit, text);
        Button_SetCheck(textCheckBox, rule->flag & RULE_REGEX_WINDOW_TEXT ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(classCheckBox, rule->flag & RULE_REGEX_WINDOW_CLASS ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(pathCheckBox, rule->flag & RULE_REGEX_EXE_FILENAME ? BST_CHECKED : BST_UNCHECKED);

        int onMinimizeChecked = BST_UNCHECKED,
            onFirstShowChecked = BST_UNCHECKED,
            onBothChecked = BST_UNCHECKED;
        if (rule->flag & RULE_ON_MINIMIZE) {
            if (rule->flag & RULE_AUTO_OFF) {
                onMinimizeChecked = BST_CHECKED;
            }
            else {
                onBothChecked = BST_CHECKED;
            }
        }
        else {
            onFirstShowChecked = BST_CHECKED;
        }
        Button_SetCheck(onMinimizeRadioBox, onMinimizeChecked);
        Button_SetCheck(onFirstShowRadioBox, onFirstShowChecked);
        Button_SetCheck(onBothRadioBox, onBothChecked);

        int neverNotifyChecked = BST_UNCHECKED,
            alwaysNotifyChecked = BST_UNCHECKED,
            notifyFirstTimeChecked = BST_UNCHECKED;
        if (rule->flag & RULE_ALWAYS_NOTIFY) {
            alwaysNotifyChecked = true;
        } else if (rule->flag & RULE_NOTIFY_FIRST_TIME) {
            notifyFirstTimeChecked = true;
        }
        else {
            neverNotifyChecked = true;
        }
        Button_SetCheck(neverNotifyRadioBox, neverNotifyChecked);
        Button_SetCheck(alwaysNotifyRadioBox, alwaysNotifyChecked);
        Button_SetCheck(notifyFirstTimeRadioBox, notifyFirstTimeChecked);
    }
    isBusy = false;
}

void RuleEditor::fill()
{
    Button_SetCheck(textCheckBox, BST_UNCHECKED);
    Button_SetCheck(classCheckBox, BST_UNCHECKED);
    Button_SetCheck(pathCheckBox, BST_UNCHECKED);
    
    auto index = ComboBox_GetItemData(windowsCombo, ComboBox_GetCurSel(windowsCombo));
    if (index >= context->iconIndex) {
        return;
    }

    auto hwnd = context->icons[index].window;
    TCHAR windowText[MAX_WINDOW_TEXT]{};
    TCHAR className[MAX_CLASS_NAME]{};
    TCHAR exeFileName[MAX_PATH]{};
    if (GetWindowText(hwnd, windowText, MAX_WINDOW_TEXT)) {
        Edit_SetText(textEdit, windowText);
    }
    if (GetClassName(hwnd, className, MAX_CLASS_NAME)) {
        Edit_SetText(classEdit, className);
    }
    if (GetWindowExeFileName(hwnd, exeFileName, MAX_PATH)) {
        Edit_SetText(pathEdit, exeFileName);
    }
}

bool RuleEditor::dirty()
{
    return isDirty;
}

bool RuleEditor::save()
{
    auto index = ListBox_GetCurSel(ruleList);
    HIDING_RULE* rule = newRule();
    if (NULL == rule) {
        return false;
    }
    if ((size_t)index >= context->hidingRules.size()) {
        context->hidingRules.push_back(rule);
    }
    else {
        auto originalRule = context->hidingRules[index];
        delete[] (BYTE*)originalRule;
        context->hidingRules[index] = rule;
    }
    ListBox_DeleteString(ruleList, index);
    ListBox_InsertString(ruleList, index, rule->ruleData);
    ListBox_SetCurSel(ruleList, index);
    select();
    clean();
    return saveRules(context);
}
