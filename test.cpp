// BITS 32
// MINREG 16
// MINHEAP 64
// MINSTACK 32

// CAL .main
// HLT

// .put_char
// OUT %TEXT, R1
// RET
extern "C" void put_char(char);

// void puts(const char *Stri) {
//   while (*Stri) {
//     put_char(*Stri);
//     Stri++;
//   }
// }

// void putNumber(int N) {
//   if (N == 0) {
//     put_char('0');
//     return;
//   }

//   unsigned int num;
//   if (N < 0) {
//     put_char('-');
//     num = static_cast<unsigned int>(-N);
//   } else {
//     num = static_cast<unsigned int>(N);
//   }

//   char buffer[10];
//   int i = 0;

//   while (num > 0) {
//     buffer[i] = (num % 10) + '0';
//     num /= 10;
//     i++;
//   }

//   for (int j = i - 1; j >= 0; j--) {
//     put_char(buffer[j]);
//   }
// }


// [[clang::noinline]] int fib(int x) {
//   int X0 = 0;
//   int X1 = 1;
//   for (int i = 0; i < x; i++) {
//     auto X0New = X1;
//     auto X1New = X0 + X1;
//     X0 = X0New;
//     X1 = X1New;
//   }
//   return X0;
// }


void putNumber(int& x){
  x *= 32;
}

int main() {
  // puts("hello world!");
  //
  int x = 32;
  putNumber(x);
  return x;
}


