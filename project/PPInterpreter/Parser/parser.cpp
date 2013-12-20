#include "parser.h"
#include "lexer.h"

#include "program.h"
#include "Creators/funccreator.h"
#include "assignment.h"
#include "readinstr.h"
#include "printinstr.h"
#include "returninstr.h"
#include "number.h"
#include "variable.h"
#include "ifinstr.h"
#include "whileinstr.h"

Program Parser::Parse(Error& error) {
    InstructionBlock program_body;
    while(!tokens_.End()) {
        ParsingResult stmt_res = ParseStmt(program_body);
        if(stmt_res == INCORRECT || !tokens_.NextTokenTypeEqualsTo(NEWLINE)) {
            error.Set(Error::SYNTAX_ER, current_line_);
            return Program();
        }
        ++current_line_;
    }
    return Program(program_body);
}

ParsingResult Parser::ParseStmt(InstructionBlock& program_body) {
    ParsingResult func_res = ParseFuncDecl();
    if(func_res != NOT_MATCHED) {
        return func_res == CORRECT ? CORRECT : INCORRECT;
    }
    ParsingResult instr_res = ParseInstruction(program_body);
    if(instr_res != NOT_MATCHED) {
        return instr_res == CORRECT ? CORRECT : INCORRECT;
    }
    return NOT_MATCHED;
}

ParsingResult Parser::ParseFuncDecl() {
    if(!tokens_.CompareValueWithRollback(KEYWORDS[0])) {
        return NOT_MATCHED;
    }
    FuncCreator func_creator;
    ParsingResult func_res = ParseFuncHeader(&Parser::CheckFuncDeclParams, func_creator);
    if(func_res != CORRECT) { return INCORRECT; }
    InstructionBlock body;
    if(!CheckBlock(body)) { return INCORRECT; }
    func_creator.CreateWithBody(body);
    return CORRECT;
}

template<class T>
ParsingResult Parser::ParseFuncHeader(bool (Parser::*ParamsChecker)(T&), T& creator) {
    tokens_.FixPosition();
    if(!tokens_.CompareTypeWithRollback(ID)) { return NOT_MATCHED; }
    creator.SetIDAndLine(tokens_.GetCurrentTokenValue(), current_line_);
    if(!tokens_.CompareTypeWithRollback(OPEN_BRACE)) {
        tokens_.RollbackToFixedPosition();
        return NOT_MATCHED;
    }
    if(!(this->*ParamsChecker)(creator) ||
       !tokens_.NextTokenTypeEqualsTo(CLOSE_BRACE)) { return INCORRECT; }
    return CORRECT;
}

bool Parser::CheckFuncDeclParams(FuncCreator& creator) {
    if(!tokens_.CompareTypeWithRollback(ID)) { return true; }
    creator.AddParam(tokens_.GetCurrentTokenValue());
    return CheckFuncDeclParamsLoop(creator) ? true : false;
}

bool Parser::CheckFuncDeclParamsLoop(FuncCreator& creator) {
    if(!tokens_.CompareTypeWithRollback(COMMA)) { return true; }
    else if(!tokens_.NextTokenTypeEqualsTo(ID)) { return false; }
    creator.AddParam(tokens_.GetCurrentTokenValue());
    return CheckFuncDeclParamsLoop(creator);
}

bool Parser::CheckFuncCallParams(FuncCallCreator& creator) {
    Expr expr;
    ParsingResult expr_res = ParseExpr(expr);
    if(expr_res != CORRECT) {
        return expr_res == NOT_MATCHED;
    }
    creator.AddParam(expr);
    return CheckFuncCallParamsLoop(creator) ? true : false;
}

bool Parser::CheckFuncCallParamsLoop(FuncCallCreator& creator) {
    if(!tokens_.CompareTypeWithRollback(COMMA)) { return true; }
    else {
        Expr expr;
        ParsingResult expr_res = ParseExpr(expr);
        if(expr_res != CORRECT) { return false; }
        creator.AddParam(expr);
    }
    return CheckFuncCallParamsLoop(creator);
}

bool Parser::CheckBlock(InstructionBlock& body) {
    if(!tokens_.NextTokenTypeEqualsTo(COLUMN) ||
       !tokens_.NextTokenTypeEqualsTo(NEWLINE))
    {
        return false;
    }
    ++current_line_;
    return CheckBlockBody(body);
}

bool Parser::CheckBlockBody(InstructionBlock& body) {
    ParsingResult instr_res = ParseInstruction(body);
    if(instr_res == INCORRECT) { return false; }
    if(!tokens_.CompareTypeWithRollback(NEWLINE)) {
       if(instr_res == CORRECT) { return false; }
       return tokens_.NextTokenValueEqualsTo(KEYWORDS[1]);
    }
    ++current_line_;
    return CheckBlockBody(body);
}

ParsingResult Parser::ParseInstruction(InstructionBlock& body) {
    ParsingResult io_res = ParseIOInstr(body);
    if(io_res != NOT_MATCHED) {
        return io_res == CORRECT ? CORRECT : INCORRECT;
    }
    ParsingResult contr_flow_res = ParseControlFlowInstr(body);
    if(contr_flow_res != NOT_MATCHED) {
        return contr_flow_res == CORRECT ? CORRECT : INCORRECT;
    }
    ParsingResult assign_res = ParseAssignment(body);
    if(assign_res != NOT_MATCHED) {
        return assign_res == CORRECT ? CORRECT : INCORRECT;
    }
    FuncCallCreator func_call_creator;
    ParsingResult func_res = ParseFuncHeader(&Parser::CheckFuncCallParams, func_call_creator);
    if(func_res != NOT_MATCHED) {
        if(func_res == INCORRECT) { return INCORRECT; }
        body.AddInstruction(PtrVisitable(new FuncCall(func_call_creator.Create())));
        return CORRECT;
    }
    ParsingResult return_expr_res = ParseReturnExpr(body);
    if(return_expr_res != NOT_MATCHED) {
        return return_expr_res == CORRECT ? CORRECT : INCORRECT;
    }
    return NOT_MATCHED;
}

ParsingResult Parser::ParseIOInstr(InstructionBlock& body) {
    if(tokens_.CompareValueWithRollback(KEYWORDS[6])) {
        if(!tokens_.CompareTypeWithRollback(ID)) { return INCORRECT; }
        body.AddInstruction(PtrVisitable(new ReadInstr(tokens_.GetCurrentTokenValue(), current_line_)));
    }
    if(tokens_.CompareValueWithRollback(KEYWORDS[5])) {
        Expr expr;
        if(ParseExpr(expr) != CORRECT) { return INCORRECT; }
        body.AddInstruction(PtrVisitable(new PrintInstr(expr, current_line_)));
        return CORRECT;
    }
    return NOT_MATCHED;
}

ParsingResult Parser::ParseControlFlowInstr(InstructionBlock& body) {
    if(!tokens_.CompareValueWithRollback(KEYWORDS[2]) &&
       !tokens_.CompareValueWithRollback(KEYWORDS[3]))
    {
        return NOT_MATCHED;
    }
    std::string instruction_type = tokens_.GetCurrentTokenValue();

    Expr left_expr;
    ParsingResult expr_res = ParseExpr(left_expr);
    if(expr_res == INCORRECT || expr_res == NOT_MATCHED) { return INCORRECT; }
    if(!tokens_.NextTokenTypeEqualsTo(COMPARISON_CHAR)) { return INCORRECT; }
    std::string comp_char = tokens_.GetCurrentTokenValue();

    Expr right_expr;
    expr_res = ParseExpr(right_expr);
    InstructionBlock cf_instr_body;
    if(expr_res == INCORRECT ||
       expr_res == NOT_MATCHED ||
       !CheckBlock(cf_instr_body))
    {
        return INCORRECT;
    }
    Condition cond(left_expr, right_expr, comp_char);
    if(instruction_type == "while") {
        WhileInstr while_instr(cond, cf_instr_body);
        body.AddInstruction(PtrVisitable(new WhileInstr(while_instr)));
    }
    else {
        IfInstr if_instr(cond, cf_instr_body);
        body.AddInstruction(PtrVisitable(new IfInstr(if_instr)));
    }
    return CORRECT;
}

ParsingResult Parser::ParseAssignment(InstructionBlock& body) {
    tokens_.FixPosition();
    if(!tokens_.CompareTypeWithRollback(ID)) { return NOT_MATCHED; }
    std::string id = tokens_.GetCurrentTokenValue();
    if(!tokens_.CompareTypeWithRollback(ASSIGN_OP)) {
        tokens_.RollbackToFixedPosition();
        return NOT_MATCHED;
    }
    Expr expr;
    ParsingResult expr_res = ParseExpr(expr);
    if(expr_res == INCORRECT) { return INCORRECT; }
    Assignment assign(id, PtrVisitable(new Expr(expr)), current_line_);
    body.AddInstruction(PtrVisitable(new Assignment(assign)));
    return CORRECT;
}

ParsingResult Parser::ParseReturnExpr(InstructionBlock& body) {
    if(!tokens_.CompareValueWithRollback(KEYWORDS[4])) { return NOT_MATCHED; }
    Expr expr;
    ParsingResult expr_res = ParseExpr(expr);
    if(expr_res != NOT_MATCHED) {
        if(expr_res == INCORRECT) { return INCORRECT; }
        ReturnInstr ri(PtrVisitable(new Expr(expr)));
        body.AddInstruction(PtrVisitable(new ReturnInstr(ri)));
        return CORRECT;
    }
    tokens_.FixPosition();
    if(!tokens_.NextTokenTypeEqualsTo(NEWLINE)) {
        return INCORRECT;
    }
    tokens_.RollbackToFixedPosition();
    return CORRECT;
}

ParsingResult Parser::ParseExpr(Expr& expr) {
    expr.SetLineNumber(current_line_);
    Term term;
    ParsingResult term_res = ParseTerm(term);
    if(term_res != CORRECT) {
        return term_res == NOT_MATCHED ? NOT_MATCHED : INCORRECT;
    }
    expr.AddTerm(term);
    return CheckExprLoop(expr) ? CORRECT : INCORRECT;
}

bool Parser::CheckExprLoop(Expr& expr) {
    if(!tokens_.CompareTypeWithRollback(PLUS_OP) &&
       !tokens_.CompareTypeWithRollback(MINUS_OP))
    {
        return true;
    }
    expr.AddOperation(tokens_.GetCurrentTokenValue());
    Term term;
    ParsingResult term_res = ParseTerm(term);
    if(term_res != CORRECT) { return false; }
    expr.AddTerm(term);
    return CheckExprLoop(expr);
}

ParsingResult Parser::ParseTerm(Term& term) {
    term.SetLineNumber(current_line_);
    ParsingResult fact_res = ParseFactor(term);
    if(fact_res != CORRECT) {
        return fact_res == NOT_MATCHED ? NOT_MATCHED : INCORRECT;
    }
    return CheckTermLoop(term) ? CORRECT : INCORRECT;
}

bool Parser::CheckTermLoop(Term& term) {
    if(!tokens_.CompareTypeWithRollback(MUL_OP) &&
       !tokens_.CompareTypeWithRollback(DIV_OP))
    {
        return true;
    }
    term.AddOperation(tokens_.GetCurrentTokenValue());
    ParsingResult fact_res = ParseFactor(term);
    if(fact_res != CORRECT) { return false; }
    return CheckTermLoop(term);
}

ParsingResult Parser::ParseFactor(Term& term) {
    if(tokens_.CompareTypeWithRollback(MINUS_OP)) { /*code to add minus to number*/ }
    if(tokens_.CompareTypeWithRollback(PLUS_OP)) { /*code to add plus to number*/ }
    if(tokens_.CompareTypeWithRollback(NUMBER)) {
        term.AddElem(PtrVisitable(new Number(tokens_.GetCurrentTokenValue())));
        return CORRECT;
    }
    FuncCallCreator func_call_creator;
    ParsingResult func_res = ParseFuncHeader(&Parser::CheckFuncCallParams, func_call_creator);
    if(func_res != NOT_MATCHED) {
        if(func_res != CORRECT) { return INCORRECT; }
        term.AddElem(PtrVisitable(new FuncCall(func_call_creator.Create())));
        return CORRECT;
    }
    if(tokens_.CompareTypeWithRollback(ID)) {
        term.AddElem(PtrVisitable(new Variable(tokens_.GetCurrentTokenValue(), current_line_)));
        return CORRECT;
    }
    if(!tokens_.CompareTypeWithRollback(OPEN_BRACE)) { return NOT_MATCHED; }
    Expr expr;
    ParsingResult expr_res = ParseExpr(expr);
    if(expr_res != CORRECT) { return INCORRECT; }
    term.AddElem(PtrVisitable(new Expr(expr)));
    return tokens_.NextTokenTypeEqualsTo(CLOSE_BRACE) ? CORRECT : INCORRECT;
}
