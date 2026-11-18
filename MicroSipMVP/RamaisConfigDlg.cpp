/*
 * MicroSIP | Excel - configuracao da conexao com o Deak para a lista de ramais.
 * Baseado no MicroSIP (http://www.microsip.org), GNU GPL v2 ou posterior.
 */

#include "stdafx.h"
#include "RamaisConfigDlg.h"
#include "Ramais.h"
#include "langpack.h"

RamaisConfigDlg::RamaisConfigDlg(CWnd* pParent /*=NULL*/)
    : CThemeDialog(RamaisConfigDlg::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(RamaisConfigDlg, CThemeDialog)
    ON_BN_CLICKED(IDC_RAMAIS_CFG_TESTAR, &RamaisConfigDlg::OnBnClickedTestar)
    ON_MESSAGE(UM_RAMAIS_TESTE, &RamaisConfigDlg::OnTesteConcluido)
END_MESSAGE_MAP()

BOOL RamaisConfigDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(this->m_hWnd);
    SetDlgItemText(IDC_RAMAIS_CFG_SERVIDOR, servidor);
    SetDlgItemText(IDC_RAMAIS_CFG_BANCO, banco.IsEmpty() ? CString(_T("Deak")) : banco);
    SetDlgItemText(IDC_RAMAIS_CFG_USUARIO, usuario);
    SetDlgItemText(IDC_RAMAIS_CFG_SENHA, senha);
    return TRUE;
}

void RamaisConfigDlg::LerCampos()
{
    GetDlgItemText(IDC_RAMAIS_CFG_SERVIDOR, servidor);
    GetDlgItemText(IDC_RAMAIS_CFG_BANCO, banco);
    GetDlgItemText(IDC_RAMAIS_CFG_USUARIO, usuario);
    GetDlgItemText(IDC_RAMAIS_CFG_SENHA, senha);
    servidor.Trim();
    banco.Trim();
    usuario.Trim();
    if (banco.IsEmpty()) {
        banco = _T("Deak");
    }
    if (usuario.IsEmpty()) {
        senha.Empty(); // autenticacao do Windows nao usa senha
    }
}

void RamaisConfigDlg::OnOK()
{
    LerCampos();
    m_geracaoTeste++; // descarta teste em andamento
    CThemeDialog::OnOK();
}

void RamaisConfigDlg::OnBnClickedTestar()
{
    LerCampos();
    RamaisConexao cx;
    cx.servidor = servidor;
    cx.banco = banco;
    cx.usuario = usuario;
    cx.senha = senha;
    if (!cx.Configurada()) {
        cx = RamaisConexao::DoRegistroDeak(); // servidor em branco: vale a conexao do ERP Deak
    }
    if (!cx.Configurada()) {
        SetDlgItemText(IDC_RAMAIS_CFG_RESULTADO, Translate(_T("Not configured")));
        return;
    }
    m_geracaoTeste++;
    GetDlgItem(IDC_RAMAIS_CFG_TESTAR)->EnableWindow(FALSE);
    SetDlgItemText(IDC_RAMAIS_CFG_RESULTADO, Translate(_T("Testing connection...")));
    RamaisConsultarEmSegundoPlano(m_hWnd, UM_RAMAIS_TESTE, m_geracaoTeste, cx);
}

LRESULT RamaisConfigDlg::OnTesteConcluido(WPARAM wParam, LPARAM lParam)
{
    RamaisResultado* res = (RamaisResultado*)lParam;
    if (!res) {
        return 0;
    }
    if (res->geracao == m_geracaoTeste) {
        GetDlgItem(IDC_RAMAIS_CFG_TESTAR)->EnableWindow(TRUE);
        CString texto;
        if (res->ok) {
            size_t n = res->lista.size();
            texto.Format(_T("%s: %u %s."), Translate(_T("Connection OK")), (unsigned)n,
                n == 1 ? Translate(_T("extension")) : Translate(_T("extensions")));
        }
        else {
            texto.Format(_T("%s: %s"), Translate(_T("Connection Failed")), (LPCTSTR)res->erro);
        }
        SetDlgItemText(IDC_RAMAIS_CFG_RESULTADO, texto);
    }
    delete res;
    return 0;
}
