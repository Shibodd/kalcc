#include <iostream>
#include "driver.hh"

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cerr << "Usage: " << argv[0] << " source" << std::endl;
    return 1;
  }

  driver drv;
  int ans = drv.parse(std::string(argv[1]));
  if (ans == 0) {
    drv.root->codegen(drv, 0);
  }
  else
    std::cout << "Error!" << std::endl;
}