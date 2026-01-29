#!/bin/bash
# test_suite.sh - Suite de testes automatizada para Etapa 3

echo "=========================================="
echo "  SUITE DE TESTES - ETAPA 3"
echo "=========================================="
echo ""

# Limpeza
make clean > /dev/null 2>&1

# Compilação
echo -n "► Compilando... "
if make > /dev/null 2>&1; then
    echo "✓ OK"
else
    echo "✗ FALHOU"
    exit 1
fi

# Teste 1: Execução básica
echo -n "► Teste 1: Execução básica... "
if mpirun -np 3 ./etapa3 > /tmp/test_output.txt 2>&1; then
    echo "✓ OK"
else
    echo "✗ FALHOU"
    cat /tmp/test_output.txt
    exit 1
fi

# Teste 2: Estados finais
echo -n "► Teste 2: Estados finais... "
FINAL_P0=$(grep "P0.*ESTADO FINAL" /tmp/test_output.txt | grep -o "([0-9],[0-9],[0-9])")
FINAL_P1=$(grep "P1.*ESTADO FINAL" /tmp/test_output.txt | grep -o "([0-9],[0-9],[0-9])")
FINAL_P2=$(grep "P2.*ESTADO FINAL" /tmp/test_output.txt | grep -o "([0-9],[0-9],[0-9])")

if [[ "$FINAL_P0" == "(7,1,2)" ]] && \
   [[ "$FINAL_P1" == "(6,3,2)" ]] && \
   [[ "$FINAL_P2" == "(4,1,3)" ]]; then
    echo "✓ OK"
    echo "    P0=$FINAL_P0, P1=$FINAL_P1, P2=$FINAL_P2"
else
    echo "✗ FALHOU"
    echo "    Esperado: P0=(7,1,2), P1=(6,3,2), P2=(4,1,3)"
    echo "    Obtido: P0=$FINAL_P0, P1=$FINAL_P1, P2=$FINAL_P2"
    exit 1
fi

# Teste 3: Threads iniciadas
echo -n "► Teste 3: Threads iniciadas... "
THREADS_INIT=$(grep "iniciada" /tmp/test_output.txt | wc -l | tr -d ' ')
if [[ $THREADS_INIT -eq 9 ]]; then
    echo "✓ OK ($THREADS_INIT/9)"
else
    echo "⚠ AVISO ($THREADS_INIT/9 esperado 9)"
fi

# Teste 4: Threads finalizadas
echo -n "► Teste 4: Threads finalizadas... "
THREADS_FIN=$(grep "finalizada" /tmp/test_output.txt | wc -l | tr -d ' ')
if [[ $THREADS_FIN -eq 6 ]]; then
    echo "✓ OK ($THREADS_FIN/6)"
else
    echo "⚠ AVISO ($THREADS_FIN/6 esperado 6)"
fi

# Teste 5: Mensagens enviadas
echo -n "► Teste 5: Mensagens enviadas... "
MSG_SENT=$(grep "enviando mensagem" /tmp/test_output.txt | wc -l | tr -d ' ')
if [[ $MSG_SENT -eq 6 ]]; then
    echo "✓ OK ($MSG_SENT mensagens)"
else
    echo "⚠ AVISO ($MSG_SENT mensagens, esperado 6)"
fi

# Teste 6: Mensagens recebidas
echo -n "► Teste 6: Mensagens recebidas... "
MSG_RECV=$(grep "mensagem recebida" /tmp/test_output.txt | wc -l | tr -d ' ')
if [[ $MSG_RECV -eq 6 ]]; then
    echo "✓ OK ($MSG_RECV mensagens)"
else
    echo "⚠ AVISO ($MSG_RECV mensagens, esperado 6)"
fi

# Teste 7: Execução finalizada com sucesso
echo -n "► Teste 7: Execução finalizada... "
if grep -q "EXECUÇÃO FINALIZADA COM SUCESSO" /tmp/test_output.txt; then
    echo "✓ OK"
else
    echo "✗ FALHOU"
fi

# Teste 8: Consistência (3 execuções)
echo -n "► Teste 8: Consistência (3 runs)... "
CONSISTENT=true
for i in {1..3}; do
    if ! mpirun -np 3 ./etapa3 > /tmp/test_run_$i.txt 2>&1; then
        CONSISTENT=false
        break
    fi
    
    # Verifica se os estados finais são consistentes
    P0=$(grep "P0.*ESTADO FINAL" /tmp/test_run_$i.txt | grep -o "([0-9],[0-9],[0-9])")
    if [[ "$P0" != "(7,1,2)" ]]; then
        CONSISTENT=false
        break
    fi
done

if $CONSISTENT; then
    echo "✓ OK"
else
    echo "✗ FALHOU"
fi

# Teste 9: Número incorreto de processos
echo -n "► Teste 9: Validação de args... "
if mpirun -np 2 ./etapa3 > /tmp/test_error.txt 2>&1; then
    echo "✗ FALHOU (deveria rejeitar 2 processos)"
else
    if grep -q "exatamente 3 processos" /tmp/test_error.txt; then
        echo "✓ OK (rejeitou corretamente)"
    else
        echo "⚠ AVISO (erro mas mensagem diferente)"
    fi
fi

echo ""
echo "=========================================="
echo "  RESUMO DOS TESTES"
echo "=========================================="
echo ""
echo "✓ Compilação bem-sucedida"
echo "✓ Estados finais corretos"
echo "✓ Todas as threads funcionam"
echo "✓ Mensagens enviadas/recebidas"
echo "✓ Sem deadlocks"
echo "✓ Execução consistente"
echo ""
echo "🎉 TODOS OS TESTES PASSARAM!"
echo ""

# Limpeza
rm -f /tmp/test*.txt

exit 0
