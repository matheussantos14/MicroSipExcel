/*
 * MicroSIP | Excel - lista de ramais do Deak (Excel Suprimentos, uso interno).
 * Baseado no MicroSIP (http://www.microsip.org), GNU GPL v2 ou posterior.
 */

#pragma once

#include "resource.h"
#include "BaseDialog.h"
#include <vector>

// Um ramal vindo da tabela Usuario do Deak
struct RamalDeak {
    CString usuario;
    CString nome;
    CString ramal;
    CString ativo;
    CString chave; // nome, ramal e usuario normalizados (minusculas, sem acento) para o filtro
};

// Parametros de conexao (MicroSIP.ini, secao [Settings], chaves ramais*)
struct RamaisConexao {
    CString servidor;
    CString banco;
    CString usuario; // vazio = autenticacao do Windows
    CString senha;
    bool doRegistroDeak = false; // veio do registro do ERP, nao do MicroSIP.ini
    bool Configurada() const { return !servidor.IsEmpty(); }
    // MicroSIP.ini; sem servidor la, a conexao que o ERP Deak grava no registro do usuario
    static RamaisConexao DaConfiguracao();
    static RamaisConexao DoRegistroDeak();
};

// Consulta sincrona; use RamaisConsultarEmSegundoPlano fora de testes
bool RamaisConsultar(const RamaisConexao& conexao, std::vector<RamalDeak>& lista, CString& erro);

// Resultado entregue a janela pela mensagem informada (LPARAM = RamaisResultado*, quem recebe apaga)
struct RamaisResultado {
    UINT geracao = 0;
    bool ok = false;
    std::vector<RamalDeak> lista;
    CString erro;
};
void RamaisConsultarEmSegundoPlano(HWND destino, UINT mensagem, UINT geracao, const RamaisConexao& conexao);

#define UM_RAMAIS_CARREGADOS (WM_APP + 0x51)
#define UM_RAMAIS_TESTE      (WM_APP + 0x52)

class RamaisPanel :
    public CBaseDialog
{
public:
    RamaisPanel(CWnd* pParent = NULL);
    ~RamaisPanel();
    enum { IDD = IDD_RAMAIS };
    void TabFocusSet() override {};
    bool GotoTab(int i) override { return false; };
    void ProcessCommand(CString str) override {};

    void Recarregar();

protected:
    CListCtrl m_lista;
    CFont m_fonte;
    CToolTipCtrl m_toolTip;
    std::vector<RamalDeak> m_ramais;
    UINT m_geracao = 0;
    bool m_carregando = false;
    bool m_carregou = false;
    CString m_erro;
    int m_colunaOrdem = 0;
    bool m_ordemAsc = true;

    void Filtrar();
    void AjustarColunas();
    void AtualizarStatus();
    int RamalSelecionado();
    void Ligar(int indice);
    void Transferir(int indice);
    void AcaoPadrao(int indice);
    bool EmChamada();

    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();
    afx_msg void OnBnClickedAtualizar();
    afx_msg void OnFiltroAlterado();
    afx_msg void OnNMDblclkLista(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMCustomdrawLista(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnColumnclickLista(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnContextMenu(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRamaisCarregados(WPARAM wParam, LPARAM lParam);
    afx_msg void OnMenuPreencher();
    afx_msg void OnMenuLigar();
    afx_msg void OnMenuTransferir();
    afx_msg void OnMenuCopiar();
};
