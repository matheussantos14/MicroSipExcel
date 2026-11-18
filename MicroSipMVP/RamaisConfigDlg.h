/*
 * MicroSIP | Excel - configuracao da conexao com o Deak para a lista de ramais.
 * Baseado no MicroSIP (http://www.microsip.org), GNU GPL v2 ou posterior.
 */

#pragma once

#include "ThemeDialog.h"
#include "resource.h"

class RamaisConfigDlg : public CThemeDialog
{
public:
    RamaisConfigDlg(CWnd* pParent = NULL);
    enum { IDD = IDD_RAMAIS_CONFIG };

    CString servidor;
    CString banco;
    CString usuario;
    CString senha;

protected:
    UINT m_geracaoTeste = 0;
    void LerCampos();
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedTestar();
    afx_msg LRESULT OnTesteConcluido(WPARAM wParam, LPARAM lParam);
};
