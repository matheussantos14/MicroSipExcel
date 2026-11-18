/*
 * MicroSIP | Excel - lista de ramais do Deak (Excel Suprimentos, uso interno).
 * Baseado no MicroSIP (http://www.microsip.org), GNU GPL v2 ou posterior.
 *
 * A lista vem da tabela Usuario do banco Deak (SQL Server) por ODBC, com o driver
 * "SQL Server" que acompanha o Windows. A consulta roda numa thread propria com
 * timeout curto: banco fora do ar nunca trava nem impede o softphone de abrir.
 */

#include "stdafx.h"
#include "Ramais.h"
#include "mainDlg.h"
#include "settings.h"
#include "langpack.h"
#include <sql.h>
#include <sqlext.h>
#include <algorithm>

#pragma comment(lib, "odbc32.lib")

// Consulta pedida pela Excel (2026-11): ramal de 4 digitos, usuario nao bloqueado
static const wchar_t* RAMAIS_SQL =
    L"select usuario, nome, ramal, ativo from Usuario with (nolock) "
    L"where isnull(ramal, '') <> '' and len(isnull(ramal, '')) = 4 and ativo <> 'b' "
    L"order by nome";

static const SQLULEN RAMAIS_TIMEOUT_LOGIN = 5;    // segundos
static const SQLULEN RAMAIS_TIMEOUT_CONSULTA = 15; // segundos
static const UINT RAMAIS_TIMER_ATUALIZAR = 1;
static const UINT RAMAIS_INTERVALO_ATUALIZAR = 30 * 60 * 1000; // 30 minutos

//------------------------------------------------------------------------------
// Acesso ao banco
//------------------------------------------------------------------------------

static CString LerRegistroDeak(LPCWSTR valor)
{
    CString res;
    DWORD tamanho = 0;
    LPCWSTR chave = L"Software\\VB and VBA Program Settings\\DEAK Sistemas\\Banco Dados";
    if (RegGetValueW(HKEY_CURRENT_USER, chave, valor, RRF_RT_REG_SZ, NULL, NULL, &tamanho) == ERROR_SUCCESS && tamanho > 0) {
        if (RegGetValueW(HKEY_CURRENT_USER, chave, valor, RRF_RT_REG_SZ, NULL, res.GetBuffer(tamanho / sizeof(wchar_t) + 1), &tamanho) == ERROR_SUCCESS) {
            res.ReleaseBuffer();
        }
        else {
            res.ReleaseBuffer(0);
        }
    }
    res.Trim();
    return res;
}

// O ERP Deak (VB6, SaveSetting) grava a conexao em
// HKCU\Software\VB and VBA Program Settings\DEAK Sistemas\Banco Dados
RamaisConexao RamaisConexao::DoRegistroDeak()
{
    RamaisConexao cx;
    cx.servidor = LerRegistroDeak(L"Nome Servidor");
    cx.banco = LerRegistroDeak(L"Nome Banco");
    cx.usuario = LerRegistroDeak(L"User Id do Bando de Dados"); // "Bando" e o nome real da chave
    cx.senha = LerRegistroDeak(L"Senha Banco de Dados");
    if (cx.banco.IsEmpty()) {
        cx.banco = _T("Deak");
    }
    cx.doRegistroDeak = true;
    return cx;
}

RamaisConexao RamaisConexao::DaConfiguracao()
{
    if (CString(accountSettings.ramaisServer).Trim().IsEmpty()) {
        return DoRegistroDeak();
    }
    RamaisConexao cx;
    cx.servidor = accountSettings.ramaisServer;
    cx.banco = accountSettings.ramaisDatabase;
    cx.usuario = accountSettings.ramaisUser;
    cx.senha = accountSettings.ramaisPassword;
    cx.servidor.Trim();
    cx.banco.Trim();
    cx.usuario.Trim();
    if (cx.banco.IsEmpty()) {
        cx.banco = _T("Deak");
    }
    return cx;
}

// Valor de atributo ODBC: entre chaves quando tem caractere especial
static CString OdbcValor(const CString& valor)
{
    if (valor.FindOneOf(_T(";{}=")) == -1 && valor == CString(valor).Trim()) {
        return valor;
    }
    CString s = valor;
    s.Replace(_T("}"), _T("}}"));
    return _T("{") + s + _T("}");
}

static CString MontarConexao(const RamaisConexao& cx)
{
    CString s;
    s.Format(_T("DRIVER={SQL Server};SERVER=%s;DATABASE=%s;APP=%s;"),
        (LPCTSTR)OdbcValor(cx.servidor),
        (LPCTSTR)OdbcValor(cx.banco),
        _T("MicroSIP Excel"));
    if (cx.usuario.IsEmpty()) {
        s += _T("Trusted_Connection=Yes;");
    }
    else {
        s.AppendFormat(_T("UID=%s;PWD=%s;"), (LPCTSTR)OdbcValor(cx.usuario), (LPCTSTR)OdbcValor(cx.senha));
    }
    return s;
}

// Mensagens do driver sem o prefixo "[Microsoft][ODBC SQL Server Driver][SQL Server]"
static CString DiagnosticoOdbc(SQLSMALLINT tipo, SQLHANDLE handle)
{
    CString res;
    for (SQLSMALLINT i = 1; i <= 3; i++) {
        SQLWCHAR estado[6] = { 0 };
        SQLWCHAR msg[1024] = { 0 };
        SQLINTEGER nativo = 0;
        SQLSMALLINT len = 0;
        if (!SQL_SUCCEEDED(SQLGetDiagRecW(tipo, handle, i, estado, &nativo, msg, (SQLSMALLINT)_countof(msg), &len))) {
            break;
        }
        CString linha = (LPCWSTR)msg;
        while (linha.Left(1) == _T("[")) {
            int fim = linha.Find(_T(']'));
            if (fim == -1) {
                break;
            }
            linha = linha.Mid(fim + 1);
        }
        linha.Trim();
        if (!linha.IsEmpty() && res.Find(linha) == -1) {
            if (!res.IsEmpty()) {
                res += _T(" ");
            }
            res += linha;
        }
    }
    return res;
}

static CString LerColuna(SQLHSTMT stmt, SQLUSMALLINT coluna)
{
    CString valor;
    wchar_t buf[256];
    for (;;) {
        SQLLEN ind = 0;
        SQLRETURN r = SQLGetData(stmt, coluna, SQL_C_WCHAR, buf, sizeof(buf), &ind);
        if (r == SQL_NO_DATA || !SQL_SUCCEEDED(r) || ind == SQL_NULL_DATA) {
            break;
        }
        valor += buf;
        if (r == SQL_SUCCESS) {
            break; // SQL_SUCCESS_WITH_INFO = texto truncado, ainda ha mais
        }
    }
    valor.Trim();
    return valor;
}

// Minusculas e sem acento: "joao" acha o nome com til, "conceicao" acha o com cedilha
static CString ChaveBusca(const CString& texto)
{
    CString res;
    if (texto.IsEmpty()) {
        return res;
    }
    int n = FoldStringW(MAP_COMPOSITE, texto, -1, NULL, 0);
    CString decomposto;
    if (n > 0 && FoldStringW(MAP_COMPOSITE, texto, -1, decomposto.GetBuffer(n), n) > 0) {
        decomposto.ReleaseBuffer();
    }
    else {
        decomposto.ReleaseBuffer(0);
        decomposto = texto;
    }
    for (int i = 0; i < decomposto.GetLength(); i++) {
        wchar_t c = decomposto[i];
        if (c >= 0x0300 && c <= 0x036F) {
            continue; // acento combinante
        }
        res += c;
    }
    res.MakeLower();
    return res;
}

bool RamaisConsultar(const RamaisConexao& cx, std::vector<RamalDeak>& lista, CString& erro)
{
    lista.clear();
    erro.Empty();
    if (!cx.Configurada()) {
        erro = Translate(_T("Not configured"));
        return false;
    }
    bool ok = false;
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env))) {
        erro = _T("ODBC");
        return false;
    }
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc))) {
        SQLSetConnectAttr(dbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)RAMAIS_TIMEOUT_LOGIN, 0);
        SQLSetConnectAttr(dbc, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)RAMAIS_TIMEOUT_CONSULTA, 0);
        SQLSetConnectAttr(dbc, SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
        CString conexao = MontarConexao(cx);
        SQLRETURN r = SQLDriverConnectW(dbc, NULL, (SQLWCHAR*)conexao.GetString(), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
        SecureZeroMemory(conexao.GetBuffer(), conexao.GetLength() * sizeof(wchar_t));
        conexao.ReleaseBuffer(0);
        if (SQL_SUCCEEDED(r)) {
            if (SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
                SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)RAMAIS_TIMEOUT_CONSULTA, 0);
                r = SQLExecDirectW(stmt, (SQLWCHAR*)RAMAIS_SQL, SQL_NTS);
                if (SQL_SUCCEEDED(r)) {
                    while (SQL_SUCCEEDED(r = SQLFetch(stmt))) {
                        RamalDeak item;
                        item.usuario = LerColuna(stmt, 1);
                        item.nome = LerColuna(stmt, 2);
                        item.ramal = LerColuna(stmt, 3);
                        item.ativo = LerColuna(stmt, 4);
                        if (item.nome.IsEmpty()) {
                            item.nome = item.usuario;
                        }
                        item.chave = ChaveBusca(item.nome + _T(" ") + item.ramal + _T(" ") + item.usuario);
                        lista.push_back(item);
                    }
                    ok = (r == SQL_NO_DATA);
                    if (!ok) {
                        erro = DiagnosticoOdbc(SQL_HANDLE_STMT, stmt);
                    }
                }
                else {
                    erro = DiagnosticoOdbc(SQL_HANDLE_STMT, stmt);
                }
                SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            }
            SQLDisconnect(dbc);
        }
        else {
            erro = DiagnosticoOdbc(SQL_HANDLE_DBC, dbc);
        }
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    if (!ok && erro.IsEmpty()) {
        erro = Translate(_T("Connection Failed"));
    }
    if (!ok) {
        lista.clear();
    }
    return ok;
}

struct RamaisRequisicao {
    HWND destino;
    UINT mensagem;
    RamaisConexao conexao;
    RamaisResultado* resultado;
};

static UINT AFX_CDECL RamaisThread(LPVOID param)
{
    RamaisRequisicao* req = (RamaisRequisicao*)param;
    RamaisResultado* res = req->resultado;
    res->ok = RamaisConsultar(req->conexao, res->lista, res->erro);
    if (!::IsWindow(req->destino) || !::PostMessage(req->destino, req->mensagem, 0, (LPARAM)res)) {
        delete res; // janela fechou antes da resposta
    }
    delete req;
    return 0;
}

void RamaisConsultarEmSegundoPlano(HWND destino, UINT mensagem, UINT geracao, const RamaisConexao& conexao)
{
    RamaisRequisicao* req = new RamaisRequisicao;
    req->destino = destino;
    req->mensagem = mensagem;
    req->conexao = conexao;
    req->resultado = new RamaisResultado;
    req->resultado->geracao = geracao;
    if (!AfxBeginThread(RamaisThread, req, THREAD_PRIORITY_BELOW_NORMAL)) {
        req->resultado->erro = _T("thread");
        if (!::PostMessage(destino, mensagem, 0, (LPARAM)req->resultado)) {
            delete req->resultado;
        }
        delete req;
    }
}

//------------------------------------------------------------------------------
// Painel
//------------------------------------------------------------------------------

RamaisPanel::RamaisPanel(CWnd* pParent /*=NULL*/)
    : CBaseDialog(RamaisPanel::IDD, pParent)
{
    Create(IDD, pParent);
}

RamaisPanel::~RamaisPanel()
{
}

void RamaisPanel::DoDataExchange(CDataExchange* pDX)
{
    CBaseDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RAMAIS_LISTA, m_lista);
}

BEGIN_MESSAGE_MAP(RamaisPanel, CBaseDialog)
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_BN_CLICKED(IDOK, &RamaisPanel::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &RamaisPanel::OnBnClickedCancel)
    ON_BN_CLICKED(IDC_RAMAIS_ATUALIZAR, &RamaisPanel::OnBnClickedAtualizar)
    ON_EN_CHANGE(IDC_RAMAIS_FILTRO, &RamaisPanel::OnFiltroAlterado)
    ON_NOTIFY(NM_DBLCLK, IDC_RAMAIS_LISTA, &RamaisPanel::OnNMDblclkLista)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_RAMAIS_LISTA, &RamaisPanel::OnNMCustomdrawLista)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_RAMAIS_LISTA, &RamaisPanel::OnLvnColumnclickLista)
    ON_MESSAGE(WM_CONTEXTMENU, &RamaisPanel::OnContextMenu)
    ON_MESSAGE(UM_RAMAIS_CARREGADOS, &RamaisPanel::OnRamaisCarregados)
    ON_COMMAND(ID_RAMAIS_FILL, &RamaisPanel::OnMenuPreencher)
    ON_COMMAND(ID_RAMAIS_CALL, &RamaisPanel::OnMenuLigar)
    ON_COMMAND(ID_RAMAIS_TRANSFER, &RamaisPanel::OnMenuTransferir)
    ON_COMMAND(ID_RAMAIS_COPY, &RamaisPanel::OnMenuCopiar)
END_MESSAGE_MAP()

BOOL RamaisPanel::OnInitDialog()
{
    CBaseDialog::OnInitDialog();

    AutoMove(IDC_RAMAIS_SEARCH_PICTURE, 0, 0, 0, 0);
    AutoMove(IDC_RAMAIS_FILTRO, 0, 0, 100, 0);
    AutoMove(IDC_RAMAIS_ATUALIZAR, 100, 0, 0, 0);
    AutoMove(IDC_RAMAIS_LISTA, 0, 0, 100, 100);
    AutoMove(IDC_RAMAIS_STATUS, 0, 100, 100, 0);

    TranslateDialog(this->m_hWnd);

    // mesma fonte da aba Contatos
    LOGFONT lf;
    m_lista.GetFont()->GetLogFont(&lf);
    lf.lfHeight = -MulDiv(12, dpiY, 96);
    m_fonte.CreateFontIndirect(&lf);
    m_lista.SetFont(&m_fonte);
    GetDlgItem(IDC_RAMAIS_FILTRO)->SetFont(&m_fonte);
    ((CEdit*)GetDlgItem(IDC_RAMAIS_FILTRO))->SetCueBanner(Translate(_T("Filter by name or extension")), TRUE);

    m_lista.SetExtendedStyle(m_lista.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_lista.InsertColumn(0, Translate(_T("Name")), LVCFMT_LEFT, MulDiv(200, dpiY, 96));
    m_lista.InsertColumn(1, Translate(_T("Extension")), LVCFMT_LEFT, MulDiv(60, dpiY, 96));
    AjustarColunas();

    m_toolTip.Create(this);
    m_toolTip.SetMaxTipWidth(MulDiv(400, dpiY, 96));
    m_toolTip.AddTool(GetDlgItem(IDC_RAMAIS_STATUS), Translate(_T("Double-click to fill the dialer")));
    m_toolTip.Activate(TRUE);

    SetTimer(RAMAIS_TIMER_ATUALIZAR, RAMAIS_INTERVALO_ATUALIZAR, NULL);
    Recarregar();
    return TRUE;
}

void RamaisPanel::Recarregar()
{
    if (!::IsWindow(m_hWnd)) {
        return;
    }
    RamaisConexao cx = RamaisConexao::DaConfiguracao();
    if (!cx.Configurada()) {
        return;
    }
    m_geracao++;
    m_carregando = true;
    GetDlgItem(IDC_RAMAIS_ATUALIZAR)->EnableWindow(FALSE);
    AtualizarStatus();
    RamaisConsultarEmSegundoPlano(m_hWnd, UM_RAMAIS_CARREGADOS, m_geracao, cx);
}

LRESULT RamaisPanel::OnRamaisCarregados(WPARAM wParam, LPARAM lParam)
{
    RamaisResultado* res = (RamaisResultado*)lParam;
    if (!res) {
        return 0;
    }
    if (res->geracao == m_geracao) {
        m_carregando = false;
        GetDlgItem(IDC_RAMAIS_ATUALIZAR)->EnableWindow(TRUE);
        if (res->ok) {
            m_ramais.swap(res->lista);
            m_erro.Empty();
            m_carregou = true;
            // reaplica a ordem escolhida pelo usuario (a consulta ja vem por nome)
            if (m_colunaOrdem != 0 || !m_ordemAsc) {
                LRESULT r;
                NMLISTVIEW nm = {};
                nm.iSubItem = m_colunaOrdem;
                m_ordemAsc = !m_ordemAsc; // OnLvnColumnclickLista inverte de novo
                OnLvnColumnclickLista((NMHDR*)&nm, &r);
            }
            else {
                Filtrar();
            }
        }
        else {
            // mantem a lista anterior, se houver; o erro vai para a dica do status
            m_erro = res->erro;
            AtualizarStatus();
        }
    }
    delete res;
    return 0;
}

void RamaisPanel::Filtrar()
{
    CString filtro;
    GetDlgItem(IDC_RAMAIS_FILTRO)->GetWindowText(filtro);
    filtro = ChaveBusca(filtro);
    filtro.Trim();
    CStringArray termos;
    int pos = 0;
    CString termo = filtro.Tokenize(_T(" "), pos);
    while (!termo.IsEmpty()) {
        termos.Add(termo);
        termo = filtro.Tokenize(_T(" "), pos);
    }

    CString ramalSelecionado;
    int sel = RamalSelecionado();
    if (sel != -1) {
        ramalSelecionado = m_ramais[sel].ramal;
    }

    m_lista.SetRedraw(FALSE);
    m_lista.DeleteAllItems();
    int linha = 0;
    for (size_t i = 0; i < m_ramais.size(); i++) {
        const RamalDeak& r = m_ramais[i];
        bool passa = true;
        for (INT_PTR t = 0; t < termos.GetCount(); t++) {
            if (r.chave.Find(termos[t]) == -1) {
                passa = false;
                break;
            }
        }
        if (!passa) {
            continue;
        }
        int item = m_lista.InsertItem(linha, r.nome);
        m_lista.SetItemText(item, 1, r.ramal);
        m_lista.SetItemData(item, (DWORD_PTR)i);
        if (!ramalSelecionado.IsEmpty() && r.ramal == ramalSelecionado) {
            m_lista.SetItemState(item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        linha++;
    }
    m_lista.SetRedraw(TRUE);
    m_lista.Invalidate();
    AjustarColunas();
    AtualizarStatus();
}

void RamaisPanel::AjustarColunas()
{
    if (!::IsWindow(m_lista.m_hWnd)) {
        return;
    }
    CRect rc;
    m_lista.GetClientRect(&rc);
    int larguraRamal = MulDiv(60, dpiY, 96);
    int larguraNome = rc.Width() - larguraRamal;
    if (larguraNome < MulDiv(80, dpiY, 96)) {
        larguraNome = MulDiv(80, dpiY, 96);
    }
    m_lista.SetColumnWidth(0, larguraNome);
    m_lista.SetColumnWidth(1, larguraRamal);
}

void RamaisPanel::AtualizarStatus()
{
    CString texto;
    CString dica;
    if (!m_carregou) {
        if (m_carregando) {
            texto = Translate(_T("Loading extensions..."));
        }
        else if (!m_erro.IsEmpty()) {
            texto = Translate(_T("Extension list unavailable"));
            dica.Format(_T("%s\n\n%s"), (LPCTSTR)m_erro, Translate(_T("Click Refresh to try again.")));
        }
    }
    else {
        int visiveis = m_lista.GetItemCount();
        texto.Format(_T("%d %s"), visiveis, visiveis == 1 ? Translate(_T("extension")) : Translate(_T("extensions")));
        if (m_carregando) {
            texto.AppendFormat(_T(" - %s"), Translate(_T("Loading extensions...")));
        }
        else if (!m_erro.IsEmpty()) {
            dica.Format(_T("%s\n\n%s"), (LPCTSTR)m_erro, Translate(_T("Click Refresh to try again.")));
        }
    }
    GetDlgItem(IDC_RAMAIS_STATUS)->SetWindowText(texto);
    if (::IsWindow(m_toolTip.m_hWnd)) {
        m_toolTip.UpdateTipText(dica.IsEmpty() ? CString(Translate(_T("Double-click to fill the dialer"))) : dica, GetDlgItem(IDC_RAMAIS_STATUS));
    }
}

int RamaisPanel::RamalSelecionado()
{
    POSITION pos = m_lista.GetFirstSelectedItemPosition();
    if (!pos) {
        return -1;
    }
    int item = m_lista.GetNextSelectedItem(pos);
    size_t i = (size_t)m_lista.GetItemData(item);
    return i < m_ramais.size() ? (int)i : -1;
}

bool RamaisPanel::EmChamada()
{
    if (!mainDlg || !mainDlg->messagesDlg) {
        return false;
    }
    MessagesContact* contato = mainDlg->messagesDlg->GetMessageContact();
    return contato && contato->callId != -1;
}

void RamaisPanel::Ligar(int indice)
{
    if (indice < 0 || indice >= (int)m_ramais.size()) {
        return;
    }
    const RamalDeak& r = m_ramais[indice];
    mainDlg->MakeCall(r.ramal, false, false, false, r.nome);
}

void RamaisPanel::Transferir(int indice)
{
    if (indice < 0 || indice >= (int)m_ramais.size()) {
        return;
    }
    mainDlg->OpenTransferDlg(mainDlg, MSIP_ACTION_TRANSFER);
    if (mainDlg->transferDlg) {
        CWnd* numero = mainDlg->transferDlg->GetDlgItem(IDC_NUMBER);
        if (numero) {
            numero->SetWindowText(m_ramais[indice].ramal);
        }
    }
}

// Duplo clique / Enter: so preenche o numero no discador; ligar fica por conta do usuario
void RamaisPanel::AcaoPadrao(int indice)
{
    if (indice < 0 || indice >= (int)m_ramais.size() || !mainDlg->pageDialer) {
        return;
    }
    const CString& ramal = m_ramais[indice].ramal;
    CComboBox* numero = (CComboBox*)mainDlg->pageDialer->GetDlgItem(IDC_NUMBER);
    if (!numero) {
        return;
    }
    numero->SetWindowText(ramal);
    mainDlg->pageDialer->UpdateCallButton();
    // foco no "Chamar": um Enter disca. Em chamada o botao fica oculto (e o Enter poderia
    // encerrar no modo de chamada unica), entao o foco fica no numero.
    CWnd* chamar = mainDlg->pageDialer->GetDlgItem(IDC_CALL);
    if (chamar && chamar->IsWindowVisible() && chamar->IsWindowEnabled()) {
        mainDlg->pageDialer->GotoDlgCtrl(chamar);
    }
    else {
        numero->SetFocus();
        numero->SetEditSel(ramal.GetLength(), ramal.GetLength());
    }
}

void RamaisPanel::OnNMCustomdrawLista(NMHDR* pNMHDR, LRESULT* pResult)
{
    // linhas zebradas
    LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        return;
    case CDDS_ITEMPREPAINT:
        if (m_lista.GetItemState((int)cd->nmcd.dwItemSpec, LVIS_SELECTED) && GetFocus() != &m_lista) {
            // selecao continua visivel depois que o foco vai para o discador
            cd->nmcd.uItemState &= ~CDIS_SELECTED;
            cd->clrTextBk = RGB(204, 228, 247);
            cd->clrText = RGB(0, 0, 0);
        }
        else if (cd->nmcd.dwItemSpec % 2) {
            cd->clrTextBk = RGB(238, 243, 250);
        }
        *pResult = CDRF_DODEFAULT;
        return;
    }
    *pResult = CDRF_DODEFAULT;
}

void RamaisPanel::OnSize(UINT nType, int cx, int cy)
{
    CBaseDialog::OnSize(nType, cx, cy);
    AjustarColunas();
}

void RamaisPanel::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == RAMAIS_TIMER_ATUALIZAR) {
        if (!m_carregando) {
            Recarregar();
        }
        return;
    }
    CBaseDialog::OnTimer(nIDEvent);
}

BOOL RamaisPanel::PreTranslateMessage(MSG* pMsg)
{
    if (::IsWindow(m_toolTip.m_hWnd)) {
        m_toolTip.RelayEvent(pMsg);
    }
    if (pMsg->message == WM_KEYDOWN) {
        CWnd* filtro = GetDlgItem(IDC_RAMAIS_FILTRO);
        CWnd* foco = GetFocus();
        if (foco == filtro) {
            if (pMsg->wParam == VK_DOWN && m_lista.GetItemCount()) {
                m_lista.SetFocus();
                if (RamalSelecionado() == -1) {
                    m_lista.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
                return TRUE;
            }
            if (pMsg->wParam == VK_ESCAPE && filtro->GetWindowTextLength()) {
                filtro->SetWindowText(_T(""));
                return TRUE;
            }
        }
        else if (foco == &m_lista && pMsg->wParam == VK_UP) {
            POSITION pos = m_lista.GetFirstSelectedItemPosition();
            if (pos && m_lista.GetNextSelectedItem(pos) == 0) {
                filtro->SetFocus();
                return TRUE;
            }
        }
    }
    return CBaseDialog::PreTranslateMessage(pMsg);
}

void RamaisPanel::OnBnClickedOk()
{
    int indice = RamalSelecionado();
    if (indice == -1 && GetFocus() == GetDlgItem(IDC_RAMAIS_FILTRO) && m_lista.GetItemCount()) {
        indice = (int)m_lista.GetItemData(0); // Enter no filtro: primeiro resultado
    }
    if (indice != -1) {
        AcaoPadrao(indice);
    }
}

void RamaisPanel::OnBnClickedCancel()
{
    // Esc fora do filtro: mesmo comportamento das outras abas (esconde a janela)
    mainDlg->ShowWindow(SW_HIDE);
}

void RamaisPanel::OnBnClickedAtualizar()
{
    if (!m_carregando) {
        Recarregar();
    }
}

void RamaisPanel::OnFiltroAlterado()
{
    Filtrar();
}

void RamaisPanel::OnNMDblclkLista(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE item = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    if (item->iItem != -1) {
        AcaoPadrao((int)m_lista.GetItemData(item->iItem));
    }
    *pResult = 0;
}

void RamaisPanel::OnLvnColumnclickLista(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW nm = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    int coluna = nm->iSubItem == 1 ? 1 : 0;
    if (coluna == m_colunaOrdem) {
        m_ordemAsc = !m_ordemAsc;
    }
    else {
        m_colunaOrdem = coluna;
        m_ordemAsc = true;
    }
    bool asc = m_ordemAsc;
    if (coluna == 1) {
        std::stable_sort(m_ramais.begin(), m_ramais.end(), [asc](const RamalDeak& a, const RamalDeak& b) {
            int c = a.ramal.Compare(b.ramal);
            return asc ? c < 0 : c > 0;
            });
    }
    else {
        std::stable_sort(m_ramais.begin(), m_ramais.end(), [asc](const RamalDeak& a, const RamalDeak& b) {
            int c = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | LINGUISTIC_IGNOREDIACRITIC,
                a.nome, -1, b.nome, -1, NULL, NULL, 0) - CSTR_EQUAL;
            return asc ? c < 0 : c > 0;
            });
    }
    Filtrar();
    *pResult = 0;
}

LRESULT RamaisPanel::OnContextMenu(WPARAM wParam, LPARAM lParam)
{
    if ((HWND)wParam != m_lista.m_hWnd) {
        return 0;
    }
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    int indice = RamalSelecionado();
    if (x == -1 && y == -1) {
        // pelo teclado: abre junto do item selecionado
        POINT pt = { 10, 10 };
        POSITION pos = m_lista.GetFirstSelectedItemPosition();
        if (pos) {
            m_lista.GetItemPosition(m_lista.GetNextSelectedItem(pos), &pt);
            pt.x += 40;
            pt.y += 8;
        }
        m_lista.ClientToScreen(&pt);
        x = pt.x;
        y = pt.y;
    }
    if (indice == -1) {
        return 0;
    }
    CMenu menu;
    menu.CreatePopupMenu();
    bool emChamada = EmChamada();
    menu.AppendMenu(MF_STRING, ID_RAMAIS_FILL, Translate(_T("Fill Dialer")));
    menu.AppendMenu(MF_STRING, ID_RAMAIS_CALL, Translate(_T("Call")));
    menu.AppendMenu(MF_STRING | (emChamada ? 0 : MF_GRAYED), ID_RAMAIS_TRANSFER, Translate(_T("Transfer")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, ID_RAMAIS_COPY, Translate(_T("Copy Extension")));
    menu.SetDefaultItem(ID_RAMAIS_FILL);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, this);
    return 0;
}

void RamaisPanel::OnMenuPreencher()
{
    AcaoPadrao(RamalSelecionado());
}

void RamaisPanel::OnMenuLigar()
{
    Ligar(RamalSelecionado());
}

void RamaisPanel::OnMenuTransferir()
{
    Transferir(RamalSelecionado());
}

void RamaisPanel::OnMenuCopiar()
{
    int indice = RamalSelecionado();
    if (indice != -1) {
        mainDlg->CopyStringToClipboard(m_ramais[indice].ramal);
    }
}
