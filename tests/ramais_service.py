import base64
def filtrar_ramais(lista, termo):
    t = (termo or "").strip().lower()
    return [r for r in lista if r.get("ativo", 1) == 1 and (not t or t in r["nome"].lower() or t in str(r["ramal"]))]
def validar_configuracao_conexao(servidor, banco):
    if not servidor or not servidor.strip(): return False, "Usando conexao padrao do ERP Deak"
    if not banco or not banco.strip(): return False, "Nome do banco de dados obrigatorio"
    return True, servidor.strip() + " / " + banco.strip()
def cifrar_senha_ini(senha):
    return "" if not senha else base64.b64encode(senha.encode("utf-8")).decode("ascii")
def buscar_ramais_banco(conn, filtro=""):
    cur = conn.cursor()
    cur.execute("SELECT usuario, nome, ramal, ativo FROM Usuario WHERE ativo = 1")
    rows = [{"usuario": x[0], "nome": x[1], "ramal": x[2], "ativo": x[3]} for x in cur.fetchall()]
    return filtrar_ramais(rows, filtro)
