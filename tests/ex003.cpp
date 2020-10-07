#include <vector>
#include <string>
#include <memory>
#include <cstring>

/**
 * @brief Simple class for testing purposes.
 */
class Simple {
    public:
        Simple(const std::string& hello) : _hello(hello){}
    private:
        std::string _hello;
};

/**
 * @brief Convert std::string to char *
 * 
 * @param str 
 * @return char* 
 */
char * string_to_char(const std::string& str){
    char * cstr = new char [str.length()+1];
    std::strcpy (cstr, str.c_str());
    return cstr;
}

/**
 * @brief Instantiate C++ vector, unique pointer, raw pointer
 * 
 * @return int 
 */
int main() {
    /* vector - part 1 */
    std::vector<int> vec_int(0); // 0 allocation
    vec_int.push_back(1);        // 1 allocation
    vec_int.push_back(1);        // 1 allocation
    vec_int.push_back(1);        // 1 allocation

    /* vector - part 2 */
    std::vector<double> vec_double(3,1.0); // 1 allocation

    /* Simple class */
    Simple simple("hi there !"); // 0 allocation

    /* unique pointer */
    std::unique_ptr<Simple> hello(new Simple("coucou")); // 1 allocation

    /* raw pointer */
    const std::string arg = "hello";        // 0 allocation
    char * arg_char = string_to_char(arg);  // 1 allocation
    Simple * _hello = new Simple(arg_char); // 1 allocation

    delete arg_char;
    delete _hello;

    return EXIT_SUCCESS;
}