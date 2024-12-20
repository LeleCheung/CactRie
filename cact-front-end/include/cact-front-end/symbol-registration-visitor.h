//
// Created by creeper on 8/23/24.
//

#ifndef CACTRIE_CACT_PARSER_INCLUDE_CACT_PARSER_SYMBOL_REGISTRATION_VISITOR_H
#define CACTRIE_CACT_PARSER_INCLUDE_CACT_PARSER_SYMBOL_REGISTRATION_VISITOR_H
#include <cact-front-end/CactParserBaseVisitor.h>
#include <cact-front-end/symbol-registry.h>
#include <cact-front-end/cact-parser-context.h>
#include <cact-front-end/cact-operator.h>
#include <stack>


namespace cactfrontend {


// A visitor to register symbols
struct SymbolRegistrationErrorCheckVisitor : public CactParserBaseVisitor {

  // declarations
  // CactBasicType getDataType(DataTypeCtx *ctx);
  // std::vector<ConstEvalResult> getConstIniVal(ConstantInitialValueCtx *ctx);
  // bool hasConstExpr(ConstantInitialValueCtx *ctx);
  // void enterScope(observer_ptr<Scope> scope);
  // void leaveScope();
  // CactBasicType getFuncType(FunctionTypeCtx *ctx);
  // FuncParameters getFuncParams(FunctionParametersCtx *ctx);
  // FuncParameter getFuncParam(FunctionParameterCtx *ctx);

  // Constructor: initialize the symbol registry
  SymbolRegistrationErrorCheckVisitor() : registry(std::make_unique<SymbolRegistry>()) {}

  // visit a compilation unit
  std::any visitCompilationUnit(CompilationUnitCtx *ctx) override {
    currentScope = this->registry.get()->createGlobalScope();
    for (auto &child : ctx->children)
      visit(child);
    std::cout << "visitCompilationUnit() syntax check completed." << std::endl;
    return {};
  }

  std::any visitConstantDeclaration(ConstantDeclarationCtx *ctx) override {
    // record the basic type
    auto basicType = getDataType(ctx->dataType());

    // visit all constantDefinitions
    for (auto &constDef : ctx->constantDefinition()){
      constDef->needType = basicType;
      visit(constDef);
    }
    std::cout << "visitConstantDeclaration() syntax check completed." << std::endl;
    return {};
  }

  std::any visitDataType(DataTypeCtx *ctx) override {
    if (ctx->Int32())       return CactBasicType::Int32;
    else if (ctx->Bool())   return CactBasicType::Bool;
    else if (ctx->Float())  return CactBasicType::Float;
    else if (ctx->Double()) return CactBasicType::Double;
    else
      throw std::runtime_error("Invalid data type context");
    std::cout << "visitDataType() syntax check completed." << std::endl;
    return {};
  }

  CactBasicType getDataType(DataTypeCtx *ctx) {
    return std::any_cast<CactBasicType>(visit(ctx));
  }

  std::any visitConstantDefinition(ConstantDefinitionCtx *ctx) override {
    // record name and type
    ctx->name = ctx->Identifier()->getText();
    ctx->constant.constInit(ctx->needType);

    for (auto &dimCtx : ctx->IntegerConstant()) {
      int dim = std::stoi(dimCtx->getText());
      if (dim <= 0) // dimension should be positive
        throw std::runtime_error("Invalid width of array dimension");
      ctx->constant.type.addDim((uint32_t)dim);
    }

    // set some attributes for constantInitialValue, visit it and get the result
    auto constInitVal = ctx->constantInitialValue();
    constInitVal->currentDim = 0;
    constInitVal->type = ctx->constant.type;
    visit(constInitVal);

    // register the constant
    currentScope.get()->registerVariable(ctx->name, ctx->constant);

    std::cout << "visitConstantDefinition() syntax check completed." << std::endl;
    return {};
  }

  std::any visitConstantInitialValue(ConstantInitialValueCtx *ctx) override {
    // if it is a constant expression, check the value's type
    const uint32_t currentDim = ctx->currentDim;
    const CactType &type = ctx->type;
    const uint32_t typeDim = type.dim();

    auto constExpr = ctx->constantExpression();
    if (constExpr) {
      if (currentDim != typeDim)
        throw std::runtime_error("Invalid array dimension");
      visit(constExpr);
      if (type.basicType != constExpr->basicType)
        throw std::runtime_error("Invalid type of constant expression");
    }

    // if it is an array
    else {
      if (currentDim >= typeDim)
        throw std::runtime_error("Invalid array dimension");

      // count the number of child in constantInitialValue() array
      uint32_t count = ctx->constantInitialValue().size();

      // set default attributes of children -- constant initial values
      for (auto &child : ctx->constantInitialValue()) {
        child->currentDim = currentDim + 1;
        child->type = type;
      }

      // check if the initial value could be a flat array
      bool flatFlag = false;
      if (count == type.arrayDims[currentDim] && currentDim == 0) {
        flatFlag = true;
        // check for constant expressions among all children
        for (auto &child : ctx->constantInitialValue()) {
          if (!hasConstExpr(child)) {
            flatFlag = false;
            break;
          }
        }
      }

      // case (1): if the initial value is a flat array, reset currentDim of children and visit them
      if (flatFlag) {
        for (auto &child : ctx->constantInitialValue()) {
          child->currentDim = typeDim;
        }
      }
      // case (2): count result is exactly arrayDims[currentDim]
      else if (0 <= currentDim && currentDim < typeDim - 1) { // certain outer dimension
        if (count != type.arrayDims[currentDim])
          throw std::runtime_error("Invalid array width of certain dimension");
      }
      // case (3): count result is exactly arrayDims[currentDim]
      else if (currentDim == typeDim - 1) { // the innermost dimension
        if (count > type.arrayDims[currentDim])
          throw std::runtime_error("Invalid array width of certain dimension");
      }
      else {
        throw std::runtime_error("Constant value initialization: unknown error");
      }

      // visit all children
      for (auto &child : ctx->constantInitialValue()) {
        visit(child);
      }

    }
    std::cout << "visitConstantInitialValue() syntax check completed." << std::endl;
    return {};
  }

  bool hasConstExpr(ConstantInitialValueCtx *ctx) {
    return ctx->constantExpression() != nullptr;
  }

  // std::vector<ConstEvalResult> getConstIniVal(ConstantInitialValueCtx *ctx) {
  //   return std::any_cast<std::vector<ConstEvalResult>>(visit(ctx));
  // }

  std::any visitVariableDeclaration(VariableDeclarationCtx *ctx) override {
    // record the basic type
    auto basicType = getDataType(ctx->dataType());

    // visit all variableDefinitions
    for (auto &varDef : ctx->variableDefinition()){
      varDef->needType = basicType;
      visit(varDef);
    }
    std::cout << "visitVariableDeclaration() syntax check completed." << std::endl;
    return {};
  }

  // visit a variable definition
  std::any visitVariableDefinition(VariableDefinitionCtx *ctx) override {
    // record name and type
    ctx->name = ctx->Identifier()->getText();
    ctx->variable.varInit(ctx->needType);

    for (auto &dimCtx : ctx->IntegerConstant()) {
      int dim = std::stoi(dimCtx->getText());
      if (dim <= 0) // dimension should be positive
        throw std::runtime_error("Invalid width of array dimension");
      ctx->variable.type.addDim((uint32_t)dim);
    }

    // set some attributes for constantInitialValue, visit it and get the result
    auto constInitVal = ctx->constantInitialValue();
    if (constInitVal) {
      ctx->variable.initialized = true; // set initialized flag

      constInitVal->currentDim = 0;
      constInitVal->type = ctx->variable.type;
      // auto value = getConstIniVal(constInitVal);
      visit(constInitVal);
    }

    // register the constant
    currentScope.get()->registerVariable(ctx->name, ctx->variable);

    std::cout << "visitVariableDefinition() syntax check completed." << std::endl;
    return {};
  }

  // enter a new scope
  void enterScope(observer_ptr<Scope> scope) {
    scope = this->registry.get()->newScope();
    scope.get()->setParent(currentScope);
    currentScope = scope;
  }

  // leave the current scope
  void leaveScope() {
    currentScope = currentScope.get()->getParent();
  }

  // visit a function definition
  std::any visitFunctionDefinition(FunctionDefinitionCtx *ctx) override {
    // create a new scope
    enterScope(ctx->scope);

    // record the function's return data type and name 
    auto returnType = getFuncType(ctx->functionType());
    auto name = ctx->Identifier()->getText();
    currentFunction = ctx->function = this->registry.get()->newFunction(name, returnType);

    // visit the function's parameters
    auto paramsCtx = ctx->functionParameters();
    if (paramsCtx) {
      paramsCtx->function = ctx->function;
      visit(paramsCtx);
    }

    // visit the block
    visit(ctx->block());

    leaveScope();
    std::cout << "visitFunctionDefinition() syntax check completed." << std::endl;
    return {};
  }

  // get the function type
  CactBasicType getFuncType(FunctionTypeCtx *ctx) {
    return std::any_cast<CactBasicType>(visitFunctionType(ctx));
  }

  std::any visitFunctionType(FunctionTypeCtx *ctx) override {
    if      (ctx->Void())   return CactBasicType::Void;
    else if (ctx->Int32())  return CactBasicType::Int32;
    else if (ctx->Bool())   return CactBasicType::Bool;
    else if (ctx->Float())  return CactBasicType::Float;
    else if (ctx->Double()) return CactBasicType::Double;
    else
      throw std::runtime_error("Invalid function type context");
    std::cout << "visitFunctionType() syntax check completed." << std::endl;
    return {};
  }

  // FuncParameters getFuncParams(FunctionParametersCtx *ctx) {
  //   if (ctx == nullptr)
  //     return FuncParameters();
  //   return std::any_cast<FuncParameters>(visit(ctx));
  // }

  std::any visitFunctionParameters(FunctionParametersCtx *ctx) override {
    auto func = ctx->function;
    for (auto &param : ctx->functionParameter()) {
      param->function = func;
      visit(param);
    }
    std::cout << "visitFunctionParameters() syntax check completed." << std::endl;
    return {};
  }

  // FuncParameter getFuncParam(FunctionParameterCtx *ctx) {
  //   if (ctx == nullptr)
  //     return FuncParameter();
  //   return std::any_cast<FuncParameter>(visit(ctx));
  // }

  std::any visitFunctionParameter(FunctionParameterCtx *ctx) override {
    // record basic type and name
    FuncParameter param;
    auto basicType = getDataType(ctx->dataType());
    param.paramVar.paramInit(basicType);
    param.name = ctx->Identifier()->getText();

    // if the first pair of brackets is empty, push 0 to the arrayDims
    if (ctx->IntegerConstant().size() < ctx->LeftBracket().size())
      param.paramVar.type.addDim(0);

    // record the array dimensions
    for (auto &dimCtx : ctx->IntegerConstant()) {
      int dim = std::stoi(dimCtx->getText());
      if (dim <= 0) // dimension should be positive
        throw std::runtime_error("Invalid width of array dimension");
      param.paramVar.type.addDim((uint32_t)dim);
    }

    // add the parameter to the function
    ctx->function.get()->addParameter(param);

    // register the parameter
    currentScope.get()->registerVariable(param.name, param.paramVar);

    std::cout << "visitFunctionParameter() syntax check completed." << std::endl;
    return {};
  }

  // visit a block
  std::any visitBlock(BlockCtx *ctx) override {
    // create a new scope
    enterScope(ctx->scope);

    ctx->hasReturn = false;
    // visit all block items
    for (auto &item : ctx->blockItem()) {
      visit(item);
      if (item->hasReturn)
        ctx->hasReturn = true;
    }

    leaveScope();
    std::cout << "visitBlock() syntax check completed." << std::endl;
    return {};
  }

  // visit a blockItem
  std::any visitBlockItem(BlockItemCtx *ctx) override {
    ctx->hasReturn = false;

    // visit the declaration
    if (ctx->declaration()) {
      visit(ctx->declaration());
    }
    // visit the statement
    else {
      visit(ctx->statement());
      ctx->hasReturn = ctx->statement()->hasReturn;
    }

    std::cout << "visitBlockItem() syntax check completed." << std::endl;
    return {};
  }

  // visitting a statement
  std::any visitStatement(StatementCtx *ctx) override {
    // visit children[0] since statement has only one child
    visit(ctx->children[0]);

    // check return status
    if (ctx->block())
      ctx->hasReturn = ctx->block()->hasReturn;
    else if (ctx->ifStatement())
      ctx->hasReturn = ctx->ifStatement()->hasReturn;
    else if (ctx->returnStatement())
      ctx->hasReturn = true;
    else
      ctx->hasReturn = false;
    std::cout << "visitStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit a assign statement
  std::any visitAssignStatement(AssignStatementCtx *ctx) override {
    LeftValueCtx *leftValue = ctx->leftValue();
    ExpressionCtx *expression = ctx->expression();

    // an array cannot become a valid left-hand-side value
    visit(leftValue);
    if (!leftValue->validLeftValue)
      throw std::runtime_error("Variable is not a valid left value");

    // Check type compatibility
    visit(expression);
    if (leftValue->type != expression->type)
      throw std::runtime_error("Type mismatch in assignment");

    std::cout << "visitAssignStatement() syntax check completed." << std::endl;
    return {};
  }

  // visitting a expressionStatement does not need to be overrided

  // visit a return statement
  std::any visitReturnStatement(ReturnStatementCtx *ctx) override {
    // visit the expression
    auto expr = ctx->expression();
    visit(expr);

    // check return type
    if (expr == nullptr) {
      if (currentFunction.get()->returnType != CactBasicType::Void)
        throw std::runtime_error("Return type mismatch");
    }
    else {
      if (expr->type.isArray() || expr->type.basicType != currentFunction.get()->returnType)
        throw std::runtime_error("Return type mismatch");
    }

    // record function for return statement
    ctx->retFunction = currentFunction;

    std::cout << "visitReturnStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit a if statement
  std::any visitIfStatement(IfStatementCtx *ctx) override {
    // visit the condition
    visit(ctx->condition());

    // visit the statement
    visit(ctx->statement(0));
    ctx->hasReturn = ctx->statement(0)->hasReturn;

    // visit the else statement
    if (ctx->Else()) {
      // visit the statement
      visit(ctx->statement(1));
      ctx->hasReturn = ctx->hasReturn && ctx->statement(1)->hasReturn;
    }

    std::cout << "visitIfStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit while statement
  std::any visitWhileStatement(WhileStatementCtx *ctx) override {
    // visit the condition
    visit(ctx->condition());

    // visit the statement
    whileLoopStack.push(make_observer(ctx));
    visit(ctx->statement());
    whileLoopStack.pop();

    std::cout << "visitWhileStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit break statement
  std::any visitBreakStatement(BreakStatementCtx *ctx) override {
    // check if the break statement is in a loop
    if (whileLoopStack.empty())
      throw std::runtime_error("Break statement not in a loop");

    // record the loop to break
    ctx->loopToBreak = whileLoopStack.top();

    std::cout << "visitBreakStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit continue statement
  std::any visitContinueStatement(ContinueStatementCtx *ctx) override {
    // check if the continue statement is in a loop
    if (whileLoopStack.empty())
      throw std::runtime_error("Continue statement not in a loop");

    // record the loop to continue
    ctx->loopToContinue = whileLoopStack.top();

    std::cout << "visitContinueStatement() syntax check completed." << std::endl;
    return {};
  }

  // visit an expression
  std::any visitExpression(ExpressionCtx *ctx) override {
    // visit the expression
    if (ctx->addExpression()) {
      visit(ctx->addExpression());
      ctx->type = ctx->addExpression()->type;
    }
    // visit the logicalOrExpression
    else if (ctx->BooleanConstant()) {
      visit(ctx->BooleanConstant());
      ctx->type.basicType = CactBasicType::Bool;
      ctx->type.arrayDims = {};
    }
    else {
      throw std::runtime_error("Unknown error in expression");
    }

    std::cout << "visitExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a constant expression
  std::any visitConstantExpression(ConstantExpressionCtx *ctx) override {
    // visit the expression
    if (ctx->number()) {
      visit(ctx->number());
      ctx->basicType = ctx->number()->basicType;
    }
    // visit the logicalOrExpression
    else if (ctx->BooleanConstant()) {
      visit(ctx->BooleanConstant());
      ctx->basicType = CactBasicType::Bool;
    }
    else {
      throw std::runtime_error("Unknown error in expression");
    }

    std::cout << "visitConstantExpression() syntax check completed." << std::endl;
    return {};
  }

  // visitting a condition does not need to be overrided

  // visit a left value
  std::any visitLeftValue(LeftValueCtx *ctx) override {
    // record the name of lvalue, and find corresponding variable
    auto name = ctx->Identifier()->getText();
    auto var = currentScope.get()->variable(name); // if not declared, throw an error in variable()
    ctx->type = var.type;

    // visit children and check dimensions
    int count = 0;
    for (auto &expr : ctx->expression()) {
      visit(expr);
      if (!expr->type.validArrayIndex())
        throw std::runtime_error("Invalid array index type");
      count++; // count for dimensions
      if (count > var.type.dim())
        throw std::runtime_error("Indexing on a scalar");
    }

    // update the type of left value by indexing times, erasing the first count dimensions
    ctx->type.arrayDims.erase(ctx->type.arrayDims.begin(), ctx->type.arrayDims.begin() + count);
    ctx->validLeftValue = var.isValidLValue();

    std::cout << "visitLeftValue() syntax check completed." << std::endl;
    return {};
  }

  // visit primary expression
  std::any visitPrimaryExpression(PrimaryExpressionCtx *ctx) override {
    // visit the expression
    if (ctx->expression()) {
      visit(ctx->expression());
      ctx->type = ctx->expression()->type;
    }
    // visit the left value
    else if (ctx->leftValue()) {
      visit(ctx->leftValue());
      ctx->type = ctx->leftValue()->type;
    }
    // visit the number
    else if (ctx->number()) {
      visit(ctx->number());
      ctx->type.init(ctx->number()->basicType, false);
    }
    else {
      throw std::runtime_error("Unknown error in primary expression");
    }

    std::cout << "visitPrimaryExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a number
  std::any visitNumber(NumberCtx *ctx) override {
    // record the number
    auto text = ctx->getText();
    if (ctx->IntegerConstant()) {
      ctx->basicType = CactBasicType::Int32;
    }
    else if (ctx->FloatConstant()) {
      ctx->basicType = CactBasicType::Float;
    }
    else if (ctx->DoubleConstant()) {
      ctx->basicType = CactBasicType::Double;
    }
    else {
      throw std::runtime_error("Unknown error in number");
    }

    std::cout << "visitNumber() syntax check completed." << std::endl;
    return {};
  }

  // visit a unary expression
  std::any visitUnaryExpression(UnaryExpressionCtx *ctx) override {
    auto primary = ctx->primaryExpression();
    auto unary = ctx->unaryExpression();

    // -> primary
    if (primary) {
      visit(primary);
      ctx->type = primary->type;
      ctx->unaryOperator = make_observer<UnaryOperator>(new UnaryNopOperator);
    }
    // -> unaryOp unaryExpression
    else if (unary) {
      visit(unary);

      // check the type of unary expression
      if (!unary->type.validOperandType())
        throw std::runtime_error("Invalid operator type");

      if (ctx->Plus())
        ctx->unaryOperator = make_observer<UnaryOperator>(new PlusOperator);
      else if (ctx->Minus())
        ctx->unaryOperator = make_observer<UnaryOperator>(new NegOperator);
      else if (ctx->ExclamationMark())
        ctx->unaryOperator = make_observer<UnaryOperator>(new LogicalNotOperator);
      else
        throw std::runtime_error("Unknown error in unary expression");

      ctx->unaryOperator.get()->validOperandTypeCheck(unary->type);
    }
    // -> function call
    else if (ctx->Identifier()) {
      // find the function
      auto name = ctx->Identifier()->getText();
      auto func = this->registry.get()->getFunction(name);
      if (!func)
        throw std::runtime_error("Function not found");

      // check parameters inside functionArguments
      ctx->functionArguments()->needParams = func->parameters;
      visit(ctx->functionArguments());

      ctx->type.init(func->returnType, false);
    }
    else {
      throw std::runtime_error("Unknown error in unary expression");
    }

    std::cout << "visitUnaryExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a "function arguments"
  std::any visitFunctionArguments(FunctionArgumentsCtx *ctx) override {
    std::size_t numExpr = ctx->expression().size();
    std::size_t numParams = ctx->needParams.size();

    // too many arguments
    if (numExpr > numParams)
      throw std::runtime_error("Too many arguments in function call");

    if (numExpr < numParams)
      throw std::runtime_error("Expect more arguments in function call");

    // visit all functionArguments
    for (int idx = 0; idx < numExpr; idx++) {
      visit(ctx->expression()[idx]);

      // check if the argument is compatible with parameter[count]
      if (ctx->expression()[idx]->type != ctx->needParams[idx].paramVar.type)
        throw std::runtime_error("Function argument type mismatch");
    }

    std::cout << "visitFunctionArguments() syntax check completed." << std::endl;
    return {};
  }

  // visit a multiplicative expression
  std::any visitMulExpression(MulExpressionCtx *ctx) override {
    auto unary = ctx->unaryExpression();
    auto mul = ctx->mulExpression();

    // -> unaryExpression
    if (unary && !mul) {
      visit(unary);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
      ctx->type = unary->type;
    }
    // -> mulExpression binary-OP unaryExpression
    else if (unary && mul) {
      visit(mul);
      visit(unary);

      if (ctx->Asterisk())
        ctx->binaryOperator = make_observer<BinaryOperator>(new MulOperator);
      else if (ctx->Slash())
        ctx->binaryOperator = make_observer<BinaryOperator>(new DivOperator);
      else if (ctx->Percent())
        ctx->binaryOperator = make_observer<BinaryOperator>(new ModOperator);
      else
        throw std::runtime_error("Unknown error in multiplicative expression");

      ctx->binaryOperator.get()->binaryOperandCheck(mul->type, unary->type);
      ctx->type = mul->type;
    }
    else {
      throw std::runtime_error("Unknown error in multiplicative expression");
    }

    std::cout << "visitMulExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit an additive expression
  std::any visitAddExpression(AddExpressionCtx *ctx) override {
    auto add = ctx->addExpression();
    auto mul = ctx->mulExpression();

    // -> mulExpression
    if (mul && !add) {
      visit(mul);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
      ctx->type = mul->type;
    }
    // -> addExpression binary-OP mulExpression
    else if (mul && add) {
      visit(add);
      visit(mul);

      if (ctx->Plus())
        ctx->binaryOperator = make_observer<BinaryOperator>(new AddOperator);
      else if (ctx->Minus())
        ctx->binaryOperator = make_observer<BinaryOperator>(new SubOperator);
      else
        throw std::runtime_error("Unknown error in additive expression");

      ctx->binaryOperator.get()->binaryOperandCheck(add->type, mul->type);
      ctx->type = add->type;
    }
    else {
      throw std::runtime_error("Unknown error in additive expression");
    }

    std::cout << "visitAddExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a relational expression
  std::any visitRelationalExpression(RelationalExpressionCtx *ctx) override {
    auto add = ctx->addExpression();

    // -> addExpression
    if (add.size() == 1) {
      visit(add[0]);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
      ctx->basicType = add[0]->type.basicType;
    }
    // -> addExpression binary-OP addExpression
    else if (add.size() == 2) {
      visit(add[0]);
      visit(add[1]);

      if (ctx->Less())
        ctx->binaryOperator = make_observer<BinaryOperator>(new LessOperator);
      else if (ctx->LessEqual())
        ctx->binaryOperator = make_observer<BinaryOperator>(new LessEqualOperator);
      else if (ctx->Greater())
        ctx->binaryOperator = make_observer<BinaryOperator>(new GreaterOperator);
      else if (ctx->GreaterEqual())
        ctx->binaryOperator = make_observer<BinaryOperator>(new GreaterEqualOperator);
      else
        throw std::runtime_error("Unknown error in relational expression");

      ctx->binaryOperator.get()->binaryOperandCheck(add[0]->type, add[1]->type);
      ctx->basicType = CactBasicType::Bool;
    }
    else {
      throw std::runtime_error("Unknown error in relational expression");
    }

    std::cout << "visitRelationalExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a logical equal expression
  std::any visitLogicalEqualExpression(LogicalEqualExpressionCtx *ctx) override {
    auto relational = ctx->relationalExpression();

    // -> relationalExpression
    if (relational.size() == 1) {
      visit(relational[0]);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
      if (relational[0]->basicType != CactBasicType::Bool)
        throw std::runtime_error("expecting a boolean type");
    }
    // -> relationalExpression binary-OP relationalExpression
    else if (relational.size() == 2) {
      visit(relational[0]);
      visit(relational[1]);

      if (ctx->LogicalEqual())
        ctx->binaryOperator = make_observer<BinaryOperator>(new EqualOperator);
      else if (ctx->NotEqual())
        ctx->binaryOperator = make_observer<BinaryOperator>(new NotEqualOperator);
      else
        throw std::runtime_error("Unknown error in logical equal expression");

      ctx->binaryOperator.get()->binaryOperandCheck(CactType(relational[0]->basicType, false),
                                                    CactType(relational[1]->basicType, false));
    }
    else {
      throw std::runtime_error("Unknown error in logical equal expression");
    }

    std::cout << "visitLogicalEqualExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a logical and expression
  std::any visitLogicalAndExpression(LogicalAndExpressionCtx *ctx) override {
    auto logicalEqual = ctx->logicalEqualExpression();
    auto logicalAnd = ctx->logicalAndExpression();

    // -> logicalEqualExpression
    if (logicalEqual && !logicalAnd) {
      visit(logicalEqual);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
    }
    // -> logicalAndExpression && logicalEqualExpression
    else if (logicalEqual && logicalAnd) {
      visit(logicalAnd);
      visit(logicalEqual);

      ctx->binaryOperator = make_observer<BinaryOperator>(new LogicalAndOperator);
    }
    else {
      throw std::runtime_error("Unknown error in logical and expression");
    }

    std::cout << "visitLogicalAndExpression() syntax check completed." << std::endl;
    return {};
  }

  // visit a logical or expression
  std::any visitLogicalOrExpression(LogicalOrExpressionCtx *ctx) override {
    auto logicalAnd = ctx->logicalAndExpression();
    auto logicalOr = ctx->logicalOrExpression();

    // -> logicalAndExpression
    if (logicalAnd && !logicalOr) {
      visit(logicalAnd);
      ctx->binaryOperator = make_observer<BinaryOperator>(new BinaryNopOperator);
    }
    // -> logicalOrExpression || logicalAndExpression
    else if (logicalAnd && logicalOr) {
      visit(logicalOr);
      visit(logicalAnd);

      ctx->binaryOperator = make_observer<BinaryOperator>(new LogicalOrOperator);
    }
    else {
      throw std::runtime_error("Unknown error in logical or expression");
    }

    std::cout << "visitLogicalOrExpression() syntax check completed." << std::endl;
    return {};
  }

private:
  std::unique_ptr<SymbolRegistry> registry;
  observer_ptr<Scope> currentScope;
  std::stack<observer_ptr<WhileStatementCtx>> whileLoopStack;
  observer_ptr<CactFunction> currentFunction;
};

}

#endif //CACTRIE_CACT_PARSER_INCLUDE_CACT_PARSER_SYMBOL_REGISTRATION_VISITOR_H
