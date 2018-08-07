#include <windows.h>
#include <streams.h>
#include <commctrl.h>
#include <olectl.h>

#include "resource.h"
#include "uids.h"
#include "encoder.h"
#include "props.h"

#include "dbg.h"

extern GUID g_usedHapType;
extern bool g_useSnappy;
extern UINT g_chunkCount;
extern bool g_useOMP;

//######################################
// CreateInstance
// Used by the DirectShow base classes to create instances
//######################################
CUnknown *CHapEncoderProperties::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr) {
    ASSERT(phr);
    CUnknown *punk = new CHapEncoderProperties(lpunk, phr);
    if (punk == NULL) {
        if (phr) *phr = E_OUTOFMEMORY;
    }
    return punk;
}

//######################################
// Constructor
//######################################
CHapEncoderProperties::CHapEncoderProperties(LPUNKNOWN pUnk, HRESULT *phr) :
    CBasePropertyPage(NAME("HapDecoder Property Page"), pUnk, IDD_HAPDECODERPROP, IDS_TITLE)
{
    ASSERT(phr);
}

//######################################
// OnReceiveMessage
// Handles the messages for our property window
//######################################
INT_PTR CHapEncoderProperties::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg== WM_COMMAND) {
        m_bDirty = TRUE;
        if (m_pPageSite) m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        return (LRESULT) 1;
    }
    return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);
}

//######################################
// OnActivate
//######################################
HRESULT CHapEncoderProperties::OnActivate() {

	SetDlgItemInt(m_Dlg, IDC_EDIT1, g_chunkCount, FALSE);

	CheckDlgButton(m_Dlg, IDC_CHECK1, g_useSnappy);
	CheckDlgButton(m_Dlg, IDC_CHECK2, g_useOMP);
	
	CheckDlgButton(m_Dlg, IDC_RADIO1, 0);
	CheckDlgButton(m_Dlg, IDC_RADIO2, 0);
	CheckDlgButton(m_Dlg, IDC_RADIO3, 0);

	if (g_usedHapType == MEDIASUBTYPE_Hap1)
		CheckDlgButton(m_Dlg, IDC_RADIO1, 1);
	else if (g_usedHapType == MEDIASUBTYPE_Hap5)
		CheckDlgButton(m_Dlg, IDC_RADIO2, 1);
	else
		CheckDlgButton(m_Dlg, IDC_RADIO3, 1);

    return NOERROR;
}

//######################################
// OnDeactivate
//######################################
HRESULT CHapEncoderProperties::OnDeactivate(void) {
    return NOERROR;
}

//######################################
// OnApplyChanges
//######################################
HRESULT CHapEncoderProperties::OnApplyChanges() {

	BOOL ok;
	g_chunkCount = GetDlgItemInt(m_Dlg, IDC_EDIT1, &ok, FALSE);

	if (g_chunkCount < 1) g_chunkCount = 1;
	else if (g_chunkCount > 64) g_chunkCount = 64;

	g_useSnappy = IsDlgButtonChecked(m_Dlg, IDC_CHECK1);
	g_useOMP = IsDlgButtonChecked(m_Dlg, IDC_CHECK2);

	if (IsDlgButtonChecked(m_Dlg, IDC_RADIO1))
		g_usedHapType = MEDIASUBTYPE_Hap1;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO2))
		g_usedHapType = MEDIASUBTYPE_Hap5;
	else
		g_usedHapType = MEDIASUBTYPE_HapY;

    return NOERROR;
}
