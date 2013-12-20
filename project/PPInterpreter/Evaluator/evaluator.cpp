#include "evaluator.h"

#include "program.h"
#include "function.h"
#include "instructionblock.h"
#include "assignment.h"
#include "funccall.h"
#include "printinstr.h"
#include "readinstr.h"
#include "returninstr.h"
#include "variable.h"
#include "condition.h"
#include "ifinstr.h"
#include "whileinstr.h"
#include "expr.h"
#include "term.h"
#include "number.h"

#include <iostream>

int Evaluator::visit(Program *c) {
    c->body().accept(*this);
    if(error_.IsOccured()) {
        c->SetError(error_);
    }
    return 0;
}

int Evaluator::visit(Function *c) {
    int value = c->body().accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    if(return_instr_happened_) {
        return_instr_happened_ = false;
        return value;
    }
    return 0;
}

int Evaluator::visit(InstructionBlock *c) {
    for(size_t i = 0; i != c->instructions().size(); ++i) {
        int value = (c->instructions()[i])->accept(*this);
        if(error_.IsOccured()) {
            return 0;
        }
        if(return_instr_happened_) {
            return value;
        }
    }
    return 0;
}

int Evaluator::visit(Assignment *c) {
    int value = c->expr()->accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    Scope::iterator it = scope_stack_.top().find(c->id());

    PtrVisitable number(new Number(value));
    if(it == scope_stack_.top().end()) {
        scope_stack_.top().insert(std::pair<std::string, PtrVisitable>(c->id(), number));
    }
    else {
        it->second = number;
    }
    return 0;
}

int Evaluator::visit(FuncCall *c) {
    GSFuncs::iterator it = GlobalScope::GetInstance().gs_funcs.find(c->id());
    if(it == GlobalScope::GetInstance().gs_funcs.end()) {
        error_.Set(Error::UNDEFFUNC_ER, c->line_number());
        return 0;
    }
    std::vector<std::string> params_names((it->second)->params());
    if(params_names.size() != c->params().size()) {
        error_.Set(Error::ARGNUMMISMATCH_ER, c->line_number());
        return 0;
    }
    Scope local_scope;
    for(size_t i = 0; i != c->params().size(); ++i) {
        int value = (c->params()[i]).accept(*this);
        if(error_.IsOccured()) {
            return 0;
        }
        PtrVisitable number(new Number(value));
        local_scope.insert(std::pair<std::string, PtrVisitable>(params_names[i], number));
    }
    scope_stack_.push(local_scope);
    int value = (it->second)->accept(*this);
    scope_stack_.pop();
    if(error_.IsOccured()) {
        return 0;
    }
    return value;
}

int Evaluator::visit(PrintInstr *c) {
    int value = c->expr()->accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    std::cout << value;
    return 0;
}

int Evaluator::visit(ReadInstr *c) {
    Scope::iterator it = scope_stack_.top().find(c->id());
    if(it == scope_stack_.top().end()) {
        error_.Set(Error::UNDEFVAR_ER, c->line_number());
        return 0;
    }
    int input = 0;
    std::cin >> input;
    PtrVisitable number(new Number(input));
    it->second = number;
    return 0;
}

int Evaluator::visit(ReturnInstr *c) {
    int value = c->expr()->accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    return_instr_happened_ = true;
    return value;
}

int Evaluator::visit(Variable *c) {
    Scope::iterator it = scope_stack_.top().find(c->id());
    if(it == scope_stack_.top().end()) {
        error_.Set(Error::UNDEFVAR_ER, c->line_number());
        return 0;
    }
    int value = (it->second)->accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    return value;
}

int Evaluator::visit(Condition *c) {
    int value1 = c->e1().accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    int value2 = c->e2().accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    if(c->comp_char() == ">") {
        return value1 > value2 ? 1 : 0;
    }
    if(c->comp_char() == "<") {
        return value1 < value2 ? 1 : 0;
    }
    if(c->comp_char() == ">=") {
        return value1 >= value2 ? 1 : 0;
    }
    if(c->comp_char() == "<=") {
        return value1 <= value2 ? 1 : 0;
    }
    if(c->comp_char() == "==") {
        return value1 == value2 ? 1 : 0;
    }
    if(c->comp_char() == "!=") {
        return value1 != value2 ? 1 : 0;
    }
    return 0;
}

int Evaluator::visit(IfInstr *c) {
    int is_true = c->condition().accept(*this);
    if(error_.IsOccured() || !is_true) {
        return 0;
    }
    int value = c->body().accept(*this);
    if(error_.IsOccured()) {
        return 0;
    }
    return value;
}

int Evaluator::visit(WhileInstr *c) {
    int is_true;
    while((is_true = c->condition().accept(*this)) && !error_.IsOccured()) {
        int value = c->body().accept(*this);
        if(error_.IsOccured()) {
            return 0;
        }
        if(return_instr_happened_) {
            return value;
        }
    }
    return 0;
}

int Evaluator::visit(Expr *c) {
    int result = 0;
    std::string operation = "+";
    c->operations().push_back("+");
    for(size_t i = 0; i != c->elements().size(); ++i) {
        int value = (c->elements()[i])->accept(*this);
        if(error_.IsOccured()) {
            return 0;
        }
        if(operation == "+") {
            result += value;
        }
        else {
            result -= value;
        }
        operation = c->operations()[i];
    }
    return result;
}

int Evaluator::visit(Term *c) {
    int result = 1;
    std::string operation = "*";
    c->operations().push_back("*");
    for(size_t i = 0; i != c->elements().size(); ++i) {
        int value = (c->elements()[i])->accept(*this);
        if(error_.IsOccured()) {
            return 0;
        }
        if(operation == "*") {
            result *= value;
        }
        else {
            if(value == 0) {
                error_.Set(Error::DIVBYZERO_ER, c->line_number());
                return 0;
            }
            result /= value;
        }
        operation = c->operations()[i];
    }
    return result;
}

int Evaluator::visit(Number *c) {
    return c->value();
}
