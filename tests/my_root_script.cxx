void my_root_script() {

  std::cout << "Hello from my_root_script.cxx!\n";

  std::cout << "This should be run with singularity\n";
  double pi = 3.14;

  auto pi_equals_3 = (3 == pi);
  std::cout <<  " pi_equals_3 = " << pi_equals_3 << "\n";

  if( pi_equals_3) {
    std::cout << "what the hell?\n";
    std::exit( 0 );
  }
  /* else */
  
  std::exit( -1 );
}
