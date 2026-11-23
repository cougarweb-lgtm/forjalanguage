import sys

Limite = int(sys.argv[1])
Quantos = 0
for I in range(1, Limite + 1):
    if I % 15 == 0:
        print("FizzBuzz")
        Quantos += 1
    elif I % 3 == 0:
        print("Fizz")
    elif I % 5 == 0:
        print("Buzz")
    else:
        print(I)
sys.exit(Quantos)
