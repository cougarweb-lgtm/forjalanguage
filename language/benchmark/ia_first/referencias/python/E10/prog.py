import sys

A, B, C, D, X, Y = (int(T) for T in sys.argv[1:7])
print(A * X + B * Y, C * X + D * Y)
