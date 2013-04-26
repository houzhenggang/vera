//
// Copyright (C) 2006-2007 Maciej Sobczak
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "config.h"
#include "structures/SourceFiles.h"
#include "plugins/Profiles.h"
#include "plugins/Rules.h"
#include "plugins/Exclusions.h"
#include "plugins/Transformations.h"
#include "plugins/Parameters.h"
#include "plugins/Reports.h"
#include "plugins/RootDirectory.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sys/stat.h>
#include <boost/program_options.hpp>


int legacy_main(int argc, char * argv[]);

int boost_main(int argc, char * argv[])
{
    // std::cout << "boost_main" << std::endl;

    // the directory containing the profile and rule definitions
    // by default it is (in this order, first has highest precedence):
    // - VERA_ROOT (if VERA_ROOT is defined)
    // - HOME/.vera++ (if HOME is defined)
    // - current directory (if scripts and profile are present)
    // - /usr/lib/vera++/ default debian directory

    Vera::Plugins::RootDirectory::DirectoryName veraRoot(VERA_INSTALL_DIR "/lib/vera++/");

    struct stat St;
    bool isInCurrent = ( stat( "./scripts", &St ) == 0
                         && stat( "./profiles", &St ) == 0 );

    // scripts and profiles folders are inside current directory
    if (isInCurrent)
    {
        // we can override /usr/lib/vera++
        veraRoot = ".";
    }
    char * veraRootEnv = getenv("HOME");
    if (veraRootEnv != NULL)
    {
      Vera::Plugins::RootDirectory::DirectoryName veraRootTmp(veraRootEnv);
      veraRootTmp += "/.vera++";
      bool isInHome = ( stat( veraRootTmp.c_str(), &St ) == 0 );
        if (isInHome)
        {
            // We assume that if the user has a .vera++ folder in
            // their home then we can override the current
            // directory
            // We don't want to override the current directory
            // only because $HOME is defined.
            veraRoot = veraRootEnv;
            veraRoot += "/.vera++";
        }
    }
    veraRootEnv = getenv("VERA_ROOT");
    if (veraRootEnv != NULL)
    {
        veraRoot = veraRootEnv;
    }

    std::string profile = "default";
    std::string transform;
    std::string parameters;
    std::string exclusions;
    std::vector<std::string> rules;
    std::vector<std::string> params;
    std::vector<std::string> args;
    std::vector<std::string> inputs;
    // outputs
    std::vector<std::string> stdreports;
    std::vector<std::string> vcreports;
    std::vector<std::string> xmlreports;
    /** Define and parse the program options
    */
    namespace po = boost::program_options;
    po::options_description visibleOptions("Options");
    visibleOptions.add_options()
        ("profile,p", po::value(&profile), "execute all rules from the given profile")
        ("rule,R", po::value(&rules), "execute the given rule (note: the .tcl extension is added"
            " automatically. can be used many times.)")
        ("transform", po::value(&transform), "execute the given transformation")
        ("std-report,o", po::value(&stdreports), "write the standard (gcc-like) report to this"
            "file. Default is standard or error output depending on the options."
            " (note: may be used many times.)")
        ("vc-report,v", po::value(&vcreports), "write the Visual C report to this file."
            " Not used by default. (note: may be used many times.)")
        ("xml-report,x", po::value(&xmlreports), "write the XML report to this file."
            " Not used by default. (note: may be used many times.)")
        ("show-rule,s", "include rule name in each report")
        ("no-duplicate,d", "do not duplicate messages if a single rule is violated many times in a"
            " single line of code")
        ("warning,w", "reports are marked as warning and generated on error output")
        ("error,e", "reports are marked as error and generated on error output."
            " An non zero exit code is used when one or more reports are generated.")
        ("quiet,q", "don't display the reports")
        ("summary,S", "display the number of reports and the number of processed files")
        ("parameters", po::value(&parameters), "read parameters from file")
        ("param,P", po::value(&params), "provide parameters to the scripts as name=value"
            " (note: can be used many times)")
        ("exclusions", po::value(&exclusions), "read exclusions from file")
        ("input,i", po::value(&inputs), "the inputs are read from that file (note: one file per"
            " line. can be used many times.")
        ("root,r", po::value(&veraRoot), "use the given directory as the vera root directory")
        ("help,h", "show this help message and exit")
        ("version", "show vera++'s version and exit");

    po::options_description allOptions;
    allOptions.add(visibleOptions).add_options()
            ("args", po::value(&args), "input files from command line");

    po::positional_options_description positionalOptions;
    positionalOptions.add("args", -1);

    po::variables_map vm;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(allOptions)
            .positional(positionalOptions).run(), vm);
        if ( vm.count("help")  )
        {
          std::cout << "vera++ [options] [list-of-files]" << std::endl
                    << visibleOptions << std::endl;
          return EXIT_SUCCESS;
        }
        po::notify(vm); // throws on error, so do after help in case
                        // there are any problems
    }
    catch (po::error& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << visibleOptions << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("version"))
    {
        std::cout << VERA_VERSION << '\n';
        return EXIT_SUCCESS;
    }

    try
    {
        Vera::Plugins::RootDirectory::setRootDirectory(veraRoot);
        Vera::Plugins::Reports::setShowRules(vm.count("show-rule"));
        if (vm.count("warning"))
        {
            if (vm.count("error"))
            {
                std::cerr
                    << "ERROR: --warning and --error can't be used at the same time." << std::endl;
                std::cerr << visibleOptions << std::endl;
                return EXIT_FAILURE;
            }
            Vera::Plugins::Reports::setPrefix("warning");
        }
        if (vm.count("error"))
        {
            Vera::Plugins::Reports::setPrefix("error");
        }
        if (vm.count("exclusions"))
        {
            Vera::Plugins::Exclusions::setExclusions(exclusions);
        }
        if (vm.count("parameters"))
        {
            Vera::Plugins::Parameters::readFromFile(parameters);
        }
        for (std::vector<std::string>::const_iterator it = params.begin();
            it != params.end();
            ++it)
        {
            Vera::Plugins::Parameters::ParamAssoc assoc(*it);
            Vera::Plugins::Parameters::set(assoc);
        }
        if (vm.count("args"))
        {
            for (std::vector<std::string>::const_iterator it = args.begin();
                it != args.end();
                ++it)
            {
                Vera::Structures::SourceFiles::addFileName(*it);
            }
        }
        else if (vm.count("input") == 0)
        {
            // list of source files is provided on stdin
            inputs.push_back("-");
        }

        for (std::vector<std::string>::const_iterator it = inputs.begin();
            it != inputs.end();
            ++it)
        {
            if (*it == "-")
            {
                Vera::Structures::SourceFiles::FileName name;
                while (std::cin >> name)
                {
                    Vera::Structures::SourceFiles::addFileName(name);
                }
            }
            else
            {
                std::ifstream file;
                std::string name;
                file.open(it->c_str());
                if (file.fail())
                {
                    throw std::ios::failure("Can't open " + *it);
                }
                while (file >> name)
                {
                    Vera::Structures::SourceFiles::addFileName(name);
                }
                if (file.bad() || file.fail())
                {
                    throw std::ios::failure( "Can't read from " + *it);
                }
                file.close();
            }
        }

        if (vm.count("std-report") == 0 && vm.count("vc-report") == 0
            && vm.count("xml-report") == 0 && vm.count("quiet") == 0)
        {
            // no report set - use std report on std out/err
            stdreports.push_back("-");
        }

        if (rules.empty() == false)
        {
            if (vm.count("profile"))
            {
                std::cerr
                    << "ERROR: --profile and --rule can't be used at the same time." << std::endl;
                std::cerr << visibleOptions << std::endl;
                return EXIT_FAILURE;
            }
            if (vm.count("transform"))
            {
                std::cerr
                    << "ERROR: --transform and --rule can't be used at the same time." << std::endl;
                std::cerr << visibleOptions << std::endl;
                return EXIT_FAILURE;
            }
            for (std::vector<std::string>::const_iterator it = rules.begin();
                it != rules.end();
                ++it)
            {
                Vera::Plugins::Rules::executeRule(*it);
            }
        }
        else if (vm.count("transform"))
        {
            if (vm.count("profile"))
            {
                std::cerr <<
                    "ERROR: --profile and --transform can't be used at the same time." << std::endl;
                std::cerr << visibleOptions << std::endl;
                return EXIT_FAILURE;
            }
            Vera::Plugins::Transformations::executeTransformation(transform);
        }
        else
        {
            Vera::Plugins::Profiles::executeProfile(profile);
        }

        for (std::vector<std::string>::const_iterator it = stdreports.begin();
            it != stdreports.end();
            ++it)
        {
            if (*it == "-")
            {
                if (vm.count("warning") || vm.count("error"))
                {
                    Vera::Plugins::Reports::writeStd(std::cerr, vm.count("no-duplicate"));
                }
                else
                {
                  Vera::Plugins::Reports::writeStd(std::cout, vm.count("no-duplicate"));
                }
            }
            else
            {
                std::ofstream file;
                file.open(it->c_str());
                if (file.fail())
                {
                    throw std::ios::failure("Can't open " + *it);
                }
                Vera::Plugins::Reports::writeStd(file, vm.count("no-duplicate"));
                if (file.bad() || file.fail())
                {
                    throw std::ios::failure( "Can't write to " + *it);
                }
                file.close();
            }
        }
        for (std::vector<std::string>::const_iterator it = vcreports.begin();
            it != vcreports.end();
            ++it)
        {
            if (*it == "-")
            {
                if (vm.count("warning") || vm.count("error"))
                {
                    Vera::Plugins::Reports::writeVc(std::cerr, vm.count("no-duplicate"));
                }
                else
                {
                  Vera::Plugins::Reports::writeVc(std::cout, vm.count("no-duplicate"));
                }
            }
            else
            {
                std::ofstream file;
                file.open(it->c_str());
                if (file.fail())
                {
                    throw std::ios::failure("Can't open " + *it);
                }
                Vera::Plugins::Reports::writeVc(file, vm.count("no-duplicate"));
                if (file.bad() || file.fail())
                {
                    throw std::ios::failure( "Can't write to " + *it);
                }
                file.close();
            }
        }
        for (std::vector<std::string>::const_iterator it = xmlreports.begin();
            it != xmlreports.end();
            ++it)
        {
            if (*it == "-")
            {
                if (vm.count("warning") || vm.count("error"))
                {
                    Vera::Plugins::Reports::writeXml(std::cerr, vm.count("no-duplicate"));
                }
                else
                {
                  Vera::Plugins::Reports::writeXml(std::cout, vm.count("no-duplicate"));
                }
            }
            else
            {
                std::ofstream file;
                file.open(it->c_str());
                if (file.fail())
                {
                    throw std::ios::failure("Can't open " + *it);
                }
                Vera::Plugins::Reports::writeXml(file, vm.count("no-duplicate"));
                if (file.bad() || file.fail())
                {
                    throw std::ios::failure( "Can't write to " + *it);
                }
                file.close();
            }
        }

        if (vm.count("summary"))
        {
            std::cerr << Vera::Plugins::Reports::count() << " reports in "
                << Vera::Structures::SourceFiles::count() << " files." << std::endl;
        }
    }
    catch (const std::exception & e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    if (vm.count("error") && Vera::Plugins::Reports::count() !=0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}