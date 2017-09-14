//=======================================================================
// Copyright (c) 2013-2014 Baptiste Wicht.
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <fstream>
#include <sstream>

#include "recurring.hpp"
#include "args.hpp"
#include "accounts.hpp"
#include "data.hpp"
#include "guid.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "console.hpp"
#include "budget_exception.hpp"
#include "expenses.hpp"

using namespace budget;

namespace {

static data_handler<recurring> recurrings;

void show_recurrings(){
    std::vector<std::string> columns = {"ID", "Account", "Name", "Amount", "Recurs"};
    std::vector<std::vector<std::string>> contents;

    money total;

    for(auto& recurring : recurrings.data){
        contents.push_back({to_string(recurring.id), recurring.account, recurring.name, to_string(recurring.amount), recurring.recurs});

        total += recurring.amount;
    }

    if(recurrings.data.empty()){
        std::cout << "No recurring expenses" << std::endl;
    } else {
        contents.push_back({"", "", "", "", ""});
        contents.push_back({"", "", "", "Total", to_string(total)});

        display_table(columns, contents);
    }
}

} //end of anonymous namespace

void budget::recurring_module::preload(){
    load_recurrings();
    load_accounts();
    load_expenses();

    auto now = budget::local_day();

    bool changed = false;

    for (auto& recurring : recurrings.data) {
        auto l_year  = last_year(recurring);
        auto l_month = last_month(recurring, l_year);

        budget::date recurring_date(l_year, l_month, 1);

        while (!(recurring_date.year() == now.year() && recurring_date.month() == now.month())) {
            // Get to the next month
            recurring_date += budget::months(1);

            budget::expense recurring_expense;
            recurring_expense.guid    = generate_guid();
            recurring_expense.date    = recurring_date;
            recurring_expense.account = get_account(recurring.account, recurring_date.year(), recurring_date.month()).id;
            recurring_expense.amount  = recurring.amount;
            recurring_expense.name    = recurring.name;

            add_expense(std::move(recurring_expense));

            changed = true;
        }
    }

    if(changed){
        save_expenses();
    }

    internal_config_remove("recurring:last_checked");
}

void budget::recurring_module::load(){
    //No need to load anything, that have been done in the preload phase
}

void budget::recurring_module::unload(){
    save_recurrings();
}

void budget::recurring_module::handle(const std::vector<std::string>& args){
    if(args.size() == 1){
        show_recurrings();
    } else {
        auto& subcommand = args[1];

        if(subcommand == "show"){
            show_recurrings();
        } else if(subcommand == "add"){
            recurring recurring;
            recurring.guid = generate_guid();
            recurring.recurs = "monthly";

            edit_string(recurring.account, "Account", not_empty_checker(), account_checker());
            edit_string(recurring.name, "Name", not_empty_checker());
            edit_money(recurring.amount, "Amount", not_negative_checker());

            // Create the equivalent expense

            auto date = budget::local_day();

            budget::expense recurring_expense;
            recurring_expense.guid    = generate_guid();
            recurring_expense.account = get_account(recurring.account, date.year(), date.month()).id;
            recurring_expense.date    = date;
            recurring_expense.amount  = recurring.amount;
            recurring_expense.name    = recurring.name;

            add_expense(std::move(recurring_expense));

            save_expenses();

            auto id = add_data(recurrings, std::move(recurring));
            std::cout << "Recurring expense " << id << " has been created" << std::endl;
        } else if(subcommand == "delete"){
            enough_args(args, 3);

            size_t id = to_number<size_t>(args[2]);

            if(!exists(recurrings, id)){
                throw budget_exception("There are no recurring expense with id " + args[2]);
            }

            remove(recurrings, id);

            std::cout << "Recurring expense " << id << " has been deleted" << std::endl;
            std::cout << "Note: The generated expenses have not been deleted" << std::endl;
        } else if(subcommand == "edit"){
            enough_args(args, 3);

            size_t id = to_number<size_t>(args[2]);

            if(!exists(recurrings, id)){
                throw budget_exception("There are no recurring expense with id " + args[2]);
            }

            auto& recurring = get(recurrings, id);
            auto previous_recurring = recurring; // Temporary Copy

            auto now = budget::local_day();

            edit_string(recurring.account, "Account", not_empty_checker(), account_checker());
            edit_string(recurring.name, "Name", not_empty_checker());
            edit_money(recurring.amount, "Amount", not_negative_checker());

            // Update the corresponding expense

            for(auto& expense : all_expenses()){
                if(expense.date.year() == now.year() && expense.date.month() == now.month()
                        && expense.name == previous_recurring.name && expense.amount == previous_recurring.amount
                        && get_account(expense.account).name == previous_recurring.account){
                    expense.name    = recurring.name;
                    expense.amount  = recurring.amount;
                    expense.account = get_account(recurring.account, now.year(), now.month()).id;
                    break;
                }
            }

            set_expenses_changed();
            save_expenses();

            recurrings.changed = true;

            std::cout << "Recurring expense " << id << " has been modified" << std::endl;
        } else {
            throw budget_exception("Invalid subcommand \"" + subcommand + "\"");
        }
    }
}

void budget::load_recurrings(){
    load_data(recurrings, "recurrings.data");
}

void budget::save_recurrings(){
    save_data(recurrings, "recurrings.data");
}

std::ostream& budget::operator<<(std::ostream& stream, const recurring& recurring){
    return stream << recurring.id  << ':' << recurring.guid << ':' << recurring.account << ':' << recurring.name << ':' << recurring.amount << ":" << recurring.recurs;
}

void budget::migrate_recurring_1_to_2(){
    load_accounts();

    load_data(recurrings, "recurrings.data", [](const std::vector<std::string>& parts, recurring& recurring){
        recurring.id = to_number<size_t>(parts[0]);
        recurring.guid = parts[1];
        recurring.old_account = to_number<size_t>(parts[2]);
        recurring.name = parts[3];
        recurring.amount = parse_money(parts[4]);
        recurring.recurs = parts[5];
        });

    for(auto& recurring : all_recurrings()){
        recurring.account = get_account(recurring.old_account).name;
    }

    recurrings.changed = true;

    save_data(recurrings, "recurrings.data");
}

void budget::operator>>(const std::vector<std::string>& parts, recurring& recurring){
    recurring.id = to_number<size_t>(parts[0]);
    recurring.guid = parts[1];
    recurring.account = parts[2];
    recurring.name = parts[3];
    recurring.amount = parse_money(parts[4]);
    recurring.recurs = parts[5];
}

budget::year budget::first_year(const budget::recurring& recurring){
    budget::year year(1400);

    for(auto& expense : all_expenses()){
        if(expense.name == recurring.name && expense.amount == recurring.amount && get_account(expense.account).name == recurring.account){
            if(year == 1400 || expense.date.year() < year){
                year = expense.date.year();
            }
        }
    }

    return year;
}

budget::month budget::first_month(const budget::recurring& recurring, budget::year year){
    budget::month month(13);

    for(auto& expense : all_expenses()){
        if(expense.date.year() == year && expense.name == recurring.name && expense.amount == recurring.amount && get_account(expense.account).name == recurring.account){
            if(month == 13 || expense.date.month() < month){
                month = expense.date.month();
            }
        }
    }

    return month;
}

budget::year budget::last_year(const budget::recurring& recurring){
    budget::year year(1400);

    for(auto& expense : all_expenses()){
        if(expense.name == recurring.name && expense.amount == recurring.amount && get_account(expense.account).name == recurring.account){
            if(year == 1400 || expense.date.year() > year){
                year = expense.date.year();
            }
        }
    }

    return year;
}

budget::month budget::last_month(const budget::recurring& recurring, budget::year year){
    budget::month month(13);

    for(auto& expense : all_expenses()){
        if(expense.date.year() == year && expense.name == recurring.name && expense.amount == recurring.amount && get_account(expense.account).name == recurring.account){
            if(month == 13 || expense.date.month() > month){
                month = expense.date.month();
            }
        }
    }

    return month;
}

std::vector<recurring>& budget::all_recurrings(){
    return recurrings.data;
}

void budget::set_recurrings_changed(){
    recurrings.changed = true;
}

void budget::set_recurrings_next_id(size_t next_id){
    recurrings.next_id = next_id;
}
