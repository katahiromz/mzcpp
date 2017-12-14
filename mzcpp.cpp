// mzcpp.cpp --- katahiromz's C preprocessor
#include "predefined.h"

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp>

#include <iostream>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#endif

void show_version(void)
{
    std::cout << "mzcpp 0.0 by katahiromz 2017.12.13" << std::endl;
}

void show_help(void)
{
    std::cout <<
        "Usage: cpp [options] input-file.h\n"
        "Options:\n"
        "  -Dmacro        Defines a macro\n"
        "  -Dmacro=def    Defines a macro\n"
        "  -Umacro        Undefines a macro\n"
        "  -Ipath         Adds include path\n"
        "  -Spath         Adds system include path\n"
        "  -o output.txt  Sets output file\n"
        "  -dM            Output macro definitions" << std::endl;
}

template <typename T_CONTEXT>
bool setup_context(T_CONTEXT& context, int argc, char **argv)
{
    using namespace boost;

    // Language options
    context.set_language(
        wave::language_support(
            wave::support_c99 |
            wave::support_option_emit_contnewlines |
            wave::support_option_insert_whitespace |
            wave::support_option_no_character_validation |
            wave::support_option_convert_trigraphs |
            wave::support_option_single_line |
            wave::support_option_emit_line_directives |
            wave::support_option_include_guard_detection |
            wave::support_option_emit_pragma_directives));

    add_predefined_macros(context);

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            std::string str = argv[i];
            if (str == "-dM")
                continue;

            switch (argv[i][1])
            {
            case 'D':
                context.add_macro_definition(str.substr(2));
                break;
            case 'U':
                context.remove_macro_definition(str.substr(2));
                break;
            case 'I':
                context.add_include_path(&(argv[i][2]));
                break;
            case 'S':
                context.add_sysinclude_path(&(argv[i][2]));
                break;
            case 'o':
                ++i;
                break;
            default:
                std::cerr << "ERROR: invalid argument '" << str << "'\n";
                return false;
            }
        }
    }

#ifdef _WIN32
    const int MAX_ENV = 512;
    #if defined(_MSC_VER) || defined(__BORLANDC__)
        char szInclude[MAX_ENV];
        if (GetEnvironmentVariableA("INCLUDE", szInclude, MAX_ENV))
        {
            context.add_sysinclude_path(szInclude);
        }
    #elif defined(__MINGW32__) || defined(__CYGWIN__) || defined(__clang__)
        char szInclude[MAX_ENV], szHost[MAX_ENV];
        if (GetEnvironmentVariableA("MINGW_PREFIX", szInclude, MAX_ENV))
        {
            strcat(szInclude, "/include");
            context.add_sysinclude_path(szInclude);
        }
        if (GetEnvironmentVariableA("MINGW_PREFIX", szInclude, MAX_ENV) &&
            GetEnvironmentVariableA("MINGW_CHOST", szHost, MAX_ENV))
        {
            strcat(szInclude, "/");
            strcat(szInclude, szHost);
            strcat(szInclude, "/include");
            context.add_sysinclude_path(szInclude);
        }
    #endif
#else   // ndef _WIN32
    const char *path1 = getenv("CPATH");
    const char *path2 = getenv("C_INCLUDE_PATH");
    const char *path3 = getenv("CPLUS_INCLUDE_PATH");
    if (path1)
    {
        context.add_sysinclude_path(path1);
    }
    if (path2)
    {
        context.add_sysinclude_path(path2);
    }
    else if (path3)
    {
        context.add_sysinclude_path(path3);
    }
    if (!path1 && !path2 && !path3)
    {
        context.add_sysinclude_path("/usr/include");
    }
#endif  // ndef _WIN32

    return true;
}

class MyInputPolicy
{
public:
    template <typename CONTEXT_IT_T>
    class inner
    {
    public:
        static std::string readFile(const char* filePath)
        {
            // Open file
            std::ifstream fs(filePath);
            if (!fs)
            {
                std::string msg = "Cannot open file '";
                msg += (filePath == nullptr) ? "(nullptr)" : filePath;
                msg += "'.";
                throw std::runtime_error(msg.c_str());
            }

            // Read
            fs.unsetf(std::ios::skipws);
            std::string text(
                std::istreambuf_iterator<char>(fs.rdbuf()),
                std::istreambuf_iterator<char>());

            return text;
        }

        template<typename PositionT>
        static void init_iterators(
            CONTEXT_IT_T& context_it,
            const PositionT& pos,
            boost::wave::language_support language)
        {
            try
            {
                context_it.code = readFile(context_it.filename.c_str());
                context_it.code += "\n";
            }
            catch (const std::exception&)
            {
                BOOST_WAVE_THROW_CTX(
                    context_it.ctx,
                    boost::wave::preprocess_exception,
                    bad_include_file,
                    context_it.filename.c_str(),
                    pos);
                return;
            }

            typedef typename CONTEXT_IT_T::iterator_type iterator_type;
            context_it.first =
                iterator_type(
                    context_it.code.begin(),
                    context_it.code.end(),
                    PositionT(context_it.filename),
                    language);
            context_it.last = iterator_type();
        }

    protected:
        std::string code;
    };
};

typedef boost::wave::cpplexer::lex_token<> token_type;
typedef boost::wave::cpplexer::lex_iterator<token_type> TokenIterator;
typedef boost::wave::context<std::string::const_iterator, TokenIterator, MyInputPolicy> WaveContext;

std::string get_position_str(const typename token_type::position_type& pos)
{
    std::stringstream ss;
    ss << pos.get_file();
    ss << ':';
    ss << pos.get_line();
    //ss << ':';
    //ss << pos.get_column();
    std::string str = ss.str();
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '\\')
            str[i] = '/';
    }
    return str;
}

void print_definitions(WaveContext& context)
{
    bool is_function, is_predef;
    WaveContext::position_type pos;
    std::vector<token_type> params;
    WaveContext::token_sequence_type tokens;

    WaveContext::name_iterator it, end = context.macro_names_end();
    for (it = context.macro_names_begin(); it != end; ++it)
    {
        if (!context.get_macro_definition(*it, is_function, is_predef, pos,
                                      params, tokens) || is_predef)
        {
            continue;
        }

        std::cout << get_position_str(pos) << ": #define " << *it;
        if (is_function)
        {
            std::cout << "(";
            std::vector<WaveContext::token_type>::iterator it, end = params.end();
            it = params.begin();
            if (it != end)
            {
                std::cout << it->get_value();
                ++it;
                for (; it != end; ++it)
                {
                    std::cout << "," << it->get_value();
                }
            }
            std::cout << ")";
        }
        std::cout << " ";
        {
            WaveContext::token_sequence_type::iterator it, end = tokens.end();
            for (it = tokens.begin(); it != end; ++it)
            {
                std::cout << it->get_value();
            }
        }
        std::cout << "\n";
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0)
    {
        show_help();
        return 1;
    }

    if (std::strcmp(argv[1], "--version") == 0)
    {
        show_version();
        return 1;
    }

    std::string input_file, output_file;
    bool emit_definitions = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg[0] != '-')
        {
            if (input_file.empty())
            {
                input_file = arg;
            }
            else
            {
                std::cerr << "ERROR: multiple input files specified\n";
                return 2;
            }
        }
        else if (arg == "-o")
        {
            if (output_file.empty())
            {
                if (i + 1 < argc)
                {
                    output_file = argv[i + 1];
                    ++i;
                }
                else
                {
                    std::cerr << "ERROR: No argument specified for '-o'\n";
                    return 10;
                }
            }
            else
            {
                std::cerr << "ERROR: multiple output files specified\n";
                return 3;
            }
        }
        else if (arg == "-dM")
        {
            emit_definitions = true;
        }
    }

    if (input_file.empty())
    {
        std::cerr << "ERROR: No input file\n";
        return 4;
    }

    // Load source
    std::ifstream fin(input_file);
    fin.unsetf(std::ios::skipws);
    std::string code(std::istreambuf_iterator<char>(fin.rdbuf()),
                     std::istreambuf_iterator<char>());

    // Prepare context
    WaveContext context(code.begin(), code.end(), input_file.c_str());

    if (!setup_context(context, argc, argv))
        return 3;

    try
    {
        if (output_file.empty())
        {
            WaveContext::iterator_type it, end = context.end();
            for (it = context.begin(); it != end; ++it)
            {
                std::cout << it->get_value();
            }
        }
        else
        {
            std::ofstream fout(output_file);
            if (!fout.is_open())
            {
                std::cerr << "ERROR: cannot open file '" << output_file << "'\n";
                return 5;
            }

            WaveContext::iterator_type it, end = context.end();
            for (it = context.begin(); it != end; ++it)
            {
                fout << it->get_value();
            }
        }

        if (emit_definitions)
        {
            print_definitions(context);
        }
    }
    catch (const boost::wave::cpp_exception& ex)
    {
        std::cerr << ex.file_name() << ":" << ex.line_no() << ": " <<
                     ex.description() << std::endl;
        return 6;
    }

    return 0;
}
