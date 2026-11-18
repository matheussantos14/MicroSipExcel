import base64

def filtrar_ramais(lista, termo):
    termo = (termo or ''). strip().lower()
    if not termo:
        return [r for r in lista if r.get('ativo', 1) == 1]
    return [
        r for r in lista
        if r.get('ativo', 1) == 1 and (termo in r['nome'].lower() or termo in str(r['ramal']))
    ]

def validar_configuracao_conexao(servidor, banco):
    if not servidor or not servidor.strip():
        return False, 'Usando conexao padrao do ERP Deak'
    if not banco or not banco.strip():
        return False, 'Nome do banco de dados obrigatorio'
    return True, f'{servidor.strip()} / {banco.strip()}'

def cifrar_senha_ini(senha):
    if not senha:
        return ''
    return base64.b64encode(senha.encode('utf-8')).decode('ascii')

def buscar_ramais_banco(conn, filtro=''):
    cursor = conn.cursor()
    cursor.execute('SELECT usuario, nome, ramal, ativo FROM Usuario WHERE ativo = 1')
    registros = [{'usuario': row[0], 'nome': row[1], 'ramal': row[2], 'ativo': row[3]} for row in cursor.fetchall()]
    return filtrar_ramais(registros, filtro)
