import sqlite3
from ramais_service import filtrar_ramais, validar_configuracao_conexao, cifrar_senha_ini, buscar_ramais_banco
def test_unit_filtrar_ramais():
    dados = [{"nome": "Carlos Silva", "ramal": "201", "ativo": 1}, {"nome": "Mariana Souza", "ramal": "202", "ativo": 1}, {"nome": "Inativo", "ramal": "299", "ativo": 0}]
    assert len(filtrar_ramais(dados, "")) == 2
    assert filtrar_ramais(dados, "mariana")[0]["ramal"] == "202"
    assert filtrar_ramais(dados, "201")[0]["nome"] == "Carlos Silva"
def test_unit_config_e_cifragem():
    assert validar_configuracao_conexao("", "")[0] is False
    assert validar_configuracao_conexao("thor", "")[0] is False
    assert validar_configuracao_conexao("thor", "Deak") == (True, "thor / Deak")
    assert cifrar_senha_ini("") == ""
    assert cifrar_senha_ini("segredo123") != "segredo123"
def test_integration_banco_usuario():
    conn = sqlite3.connect(":memory:")
    conn.execute("CREATE TABLE Usuario (usuario TEXT, nome TEXT, ramal TEXT, ativo INTEGER)")
    conn.executemany("INSERT INTO Usuario VALUES (?, ?, ?, ?)", [("carlos", "Carlos Silva", "201", 1), ("ana", "Ana Paula", "205", 1), ("antigo", "Ex Colaborador", "210", 0)])
    res = buscar_ramais_banco(conn, "ana")
    assert len(res) == 1 and res[0]["ramal"] == "205"
