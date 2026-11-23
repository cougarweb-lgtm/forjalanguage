ITENS = [("Maca", 12, 3.5), ("Banana", 5, 12.25), ("Uva", 100, 0.99)]

print(f"{'Item':<10}{'Qtd':>5}{'Preco':>8}")
for Nome, Qtd, Preco in ITENS:
    print(f"{Nome:<10}{Qtd:>5}{Preco:>8.2f}")
print(f"{'Total':<10}{sum(Q for _, Q, _ in ITENS):>5}{sum(Q * P for _, Q, P in ITENS):>8.2f}")
