#include <strsafe.h>

class CHapEncoderProperties : public CBasePropertyPage
{

public:
    static CUnknown * WINAPI CreateInstance (LPUNKNOWN lpunk, HRESULT *phr);

private:
    INT_PTR OnReceiveMessage (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HRESULT OnActivate ();
    HRESULT OnDeactivate ();
    HRESULT OnApplyChanges ();
    CHapEncoderProperties (LPUNKNOWN lpunk, HRESULT *phr);
};
