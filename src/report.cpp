//=======================================================================
// Copyright Baptiste Wicht 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include "report.hpp"
#include "expenses.hpp"
#include "earnings.hpp"
#include "budget_exception.hpp"
#include "accounts.hpp"

using namespace budget;

namespace {

void render(std::vector<std::vector<std::string>>& graph){
    std::reverse(graph.begin(), graph.end());

    for(auto& line : graph){
        for(auto& col : line){
            std::cout << col;
        }
        std::cout << std::endl;
    }
}

void monthly_report(boost::gregorian::greg_year year){
    auto today = boost::gregorian::day_clock::local_day();

    budget::money max_expenses;
    budget::money max_earnings;
    budget::money max_balance;
    budget::money min_expenses;
    budget::money min_earnings;
    budget::money min_balance;

    auto sm = start_month(year);

    std::vector<int> expenses(13);
    std::vector<int> earnings(13);
    std::vector<int> balances(13);

    for(unsigned short i = sm + 1; i < today.month() + 1; ++i){
        boost::gregorian::greg_month month = i;

        auto accounts = all_accounts(year, month);

        budget::money total_expenses;
        budget::money total_earnings;
        budget::money total_balance;

        for(auto& account : accounts){
            auto expenses = accumulate_amount_if(all_expenses(),
                [year,month,account](budget::expense& e){return e.account == account.id && e.date.year() == year && e.date.month() == month;});
            auto earnings = accumulate_amount_if(all_earnings(),
                [year,month,account](budget::earning& e){return e.account == account.id && e.date.year() == year && e.date.month() == month;});

            total_expenses += expenses;
            total_earnings += earnings;

            auto balance = account.amount;
            balance -= expenses;
            balance += earnings;

            total_balance += balance;
        }

        expenses[month] = total_expenses.dollars();
        earnings[month] = total_earnings.dollars();
        balances[month] = total_balance.dollars();

        max_expenses = std::max(max_expenses, total_expenses);
        max_earnings = std::max(max_earnings, total_earnings);
        max_balance = std::max(max_balance, total_balance);
        min_expenses = std::min(min_expenses, total_expenses);
        min_earnings = std::min(min_earnings, total_earnings);
        min_balance = std::min(min_balance, total_balance);
    }

    auto height = terminal_height() - 9;
    auto width = terminal_width() - 6;

    //TODO compute the scale based on the data
    unsigned int scale = 1000;

    int min = 0;
    if(min_expenses.negative() || min_earnings.negative() || min_balance.negative()){
        min = std::min(min_expenses, std::min(min_earnings, min_balance)).dollars();

        std::cout << "real_min" << min << std::endl;

        min = -1 * ((std::abs(min) / scale) + 1) * scale;

        std::cout << "real_min" << min << std::endl;
    }

    unsigned int max = std::max(max_earnings, std::max(max_expenses, max_balance)).dollars();
    max = ((max / scale) + 1) * scale;

    unsigned int levels = max / scale + std::abs(min) / scale;

    unsigned int step_height = height / levels;
    unsigned int precision = scale / step_height;

    auto graph_height = 9 + step_height * levels;
    auto graph_width = 6 + (12 - sm) * 8 + (12 - sm - 1) * 2;

    std::cout << graph_height << std::endl;
    std::cout << graph_width << std::endl;

    std::vector<std::vector<std::string>> graph(graph_height, std::vector<std::string>(graph_width, " "));

    std::cout << graph.size() << std::endl;
    std::cout << graph.at(0).size() << std::endl;

    unsigned int min_index = 3;
    unsigned int zero_index = min_index + 1 + (std::abs(min) / scale) * step_height;

    for(unsigned short i = sm + 1; i < today.month() + 1; ++i){
        boost::gregorian::greg_month month = i;

        auto col_start = 7 + 10 * (i - sm - 1);

        for(int j = 0; j < expenses[month] / precision; ++j){
           graph[zero_index + j][col_start] = "\033[1;41m \033[0m";
           graph[zero_index + j][col_start + 1] = "\033[1;41m \033[0m";
        }

        col_start += 3;

        for(int j = 0; j < earnings[month] / precision; ++j){
           graph[zero_index + j][col_start] = "\033[1;42m \033[0m";
           graph[zero_index + j][col_start + 1] = "\033[1;42m \033[0m";
        }

        col_start += 3;

        std::cout << "balance" << std::endl;

        if(balances[month] >= 0){
            for(int j = 0; j < balances[month] / precision; ++j){
                graph[zero_index + j][col_start] = "\033[1;44m \033[0m";
                graph[zero_index + j][col_start + 1] = "\033[1;44m \033[0m";
            }
        } else {
            for(int j = 0; j < std::abs(balances[month]) / precision; ++j){
                graph[zero_index - 1 - j][col_start] = "\033[1;44m \033[0m";
                graph[zero_index - 1 - j][col_start + 1] = "\033[1;44m \033[0m";
            }
        }

        std::cout << "balance end" << std::endl;
    }

    render(graph);
}

} //end of anonymous namespace

void budget::report_module::load(){
    load_accounts();
    load_expenses();
    load_earnings();
}

void budget::report_module::handle(const std::vector<std::string>& args){
    auto today = boost::gregorian::day_clock::local_day();

    if(args.size() == 1){
        monthly_report(today.year());
    } else {
        auto& subcommand = args[1];

        if(subcommand == "monthly"){
            monthly_report(today.year());
        } else {
            throw budget_exception("Invalid subcommand \"" + subcommand + "\"");
        }
    }
}