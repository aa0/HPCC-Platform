/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */
#include "jliball.hpp"
#include "hql.hpp"

#include "platform.h"
#include "jlib.hpp"
#include "jexcept.hpp"
#include "jmisc.hpp"

#include "hqlexpr.hpp"

#include "hqlstmt.hpp"
#include "hqlstmt.ipp"
#include "hqlfunc.hpp"

#include "hqlcpp.ipp"
#include "hqlcatom.hpp"
#include "hqlcpputil.hpp"
#include "hqlutil.hpp"

#define CLEAR_COPY_THRESHOLD            100

//---------------------------------------------------------------------------

struct HQLCPP_API HqlBoundDefinedValue : public HqlDefinedValue
{
public:
    HqlBoundDefinedValue(IHqlExpression * _original, const CHqlBoundExpr & _bound) : HqlDefinedValue(_original)
    { bound.set(_bound); }

    virtual IHqlExpression * queryExpr() const              { return bound.expr; }
    virtual void getBound(CHqlBoundExpr & result)           { result.set(bound); }

public:
    CHqlBoundExpr       bound;
};


void HqlExprAssociation::getBound(CHqlBoundExpr & result)   
{ 
    result.expr.set(queryExpr()); 
}
    
//---------------------------------------------------------------------------

BuildCtx::BuildCtx(HqlCppInstance & _state, _ATOM section) : state(_state)
{
    init(state.ensureSection(section));
}

BuildCtx::BuildCtx(HqlCppInstance & _state) : state(_state)
{
    init(NULL);
}

BuildCtx::BuildCtx(BuildCtx & _owner) : state(_owner.state)
{
    init(_owner.curStmts);
    ignoreInput = _owner.ignoreInput;
    curPriority = _owner.curPriority;
    nextPriority = _owner.nextPriority;
    ignoreInput = false;
}

BuildCtx::BuildCtx(BuildCtx & _owner, HqlStmts * _root) : state(_owner.state)
{
    init(_root);
}


BuildCtx::~BuildCtx()
{
}

void BuildCtx::set(_ATOM section)
{
    init(state.ensureSection(section));
}

void BuildCtx::set(BuildCtx & _owner)
{
    init(_owner.curStmts);
    ignoreInput = _owner.ignoreInput;
    curPriority = _owner.curPriority;
    nextPriority = _owner.nextPriority;
    ignoreInput = false;
}

IHqlStmt * BuildCtx::addAlias(IHqlStmt * aliased)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(alias_stmt, curStmts);
    HqlStmt * cast = dynamic_cast<HqlStmt *>(aliased);
    assertex(cast);
    next->code.append(*LINK(cast));
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addAssign(IHqlExpression * target, IHqlExpression * value)
{
    if (ignoreInput)
        return NULL;

    HqlStmt * next = new HqlStmt(assign_stmt, curStmts);
    next->addExpr(LINK(target));
    next->addExpr(LINK(value));
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addAssignIncrement(IHqlExpression * target, IHqlExpression * value)
{
    if (ignoreInput)
        return NULL;

    if (value && !matchesConstantValue(value, 1))
    {
        HqlStmt * next = new HqlStmt(assigninc_stmt, curStmts);
        next->addExpr(LINK(target));
        next->addExpr(LINK(value));
        return appendSimple(next);
    }
    else
    {
        OwnedHqlExpr inc = createValue(no_postinc, target->getType(), LINK(target));
        return addExprOwn(inc.getClear());
    }
}


IHqlStmt * BuildCtx::addAssignDecrement(IHqlExpression * target, IHqlExpression * value)
{
    if (ignoreInput)
        return NULL;

    if (value && !matchesConstantValue(value, 1))
    {
        HqlStmt * next = new HqlStmt(assigndec_stmt, curStmts);
        next->addExpr(LINK(target));
        next->addExpr(LINK(value));
        return appendSimple(next);
    }
    else
    {
        OwnedHqlExpr inc = createValue(no_postdec, LINK(target->getType()), LINK(target));
        return addExprOwn(inc.getClear());
    }
}


IHqlStmt * BuildCtx::addBlock()
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(block_stmt, curStmts);
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addBreak()
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(break_stmt, curStmts);
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addCase(IHqlStmt * _owner, IHqlExpression * source)
{
    if (ignoreInput)
        return NULL;
    assertThrow(_owner->getStmt() == switch_stmt);

    HqlCompoundStmt & owner = (HqlCompoundStmt &)*_owner;
    curStmts = &owner.code;

    HqlCompoundStmt * next = new HqlCompoundStmt(case_stmt, curStmts);
    next->addExpr(LINK(source));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addContinue()
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(continue_stmt, curStmts);
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addDeclare(IHqlExpression * name, IHqlExpression * value)
{
    assertex(name->getOperator() == no_variable);
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(declare_stmt, curStmts);
    next->addExpr(LINK(name));
    if (value)
        next->addExpr(LINK(value));

    appendSimple(next);
    return next;
}


IHqlStmt * BuildCtx::addDeclareExternal(IHqlExpression * name)
{
    assertex(name->getOperator() == no_variable);
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(external_stmt, curStmts);
    next->addExpr(LINK(name));
    appendSimple(next);
    return next;
}


IHqlStmt * BuildCtx::addDeclareAssign(IHqlExpression * name, IHqlExpression * value)
{
    addDeclare(name);
    return addAssign(name, value);
}


IHqlStmt * BuildCtx::addDefault(IHqlStmt * _owner)
{
    if (ignoreInput)
        return NULL;
    assertThrow(_owner->getStmt() == switch_stmt);

    selectCompound(_owner);
    HqlCompoundStmt * next = new HqlCompoundStmt(default_stmt, curStmts);
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addExpr(IHqlExpression * value)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(expr_stmt, curStmts);
    next->addExpr(LINK(value));
    return appendSimple(next);
}

IHqlStmt * BuildCtx::addExprOwn(IHqlExpression * value)
{
    if (ignoreInput)
    {
        value->Release();
        return NULL;
    }
    HqlStmt * next = new HqlStmt(expr_stmt, curStmts);
    next->addExpr(value);
    return appendSimple(next);
}

IHqlStmt * BuildCtx::addReturn(IHqlExpression * value)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(return_stmt, curStmts);
    if (value)
        next->addExpr( LINK(value));
    return appendSimple(next);
}

IHqlStmt * BuildCtx::addFilter(IHqlExpression * condition)
{
    HqlCompoundStmt * next = new HqlCompoundStmt(filter_stmt, curStmts);
    next->addExpr(LINK(condition));
    appendCompound(next);
    addBlock();
    return next;
}


IHqlStmt * BuildCtx::addFunction(IHqlExpression * funcdef)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(function_stmt, curStmts);
    next->addExpr(LINK(funcdef));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addIndirection(const BuildCtx & _parent)
{
    HqlCompoundStmt * next = new HqlCompoundStmt(indirect_stmt, _parent.curStmts);
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addLoop(IHqlExpression * cond, IHqlExpression * inc, bool atEnd)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(loop_stmt, curStmts);
    if (cond)
        next->addExpr(LINK(cond));
    if (inc)
        next->addExpr(LINK(inc));
    if (atEnd)
        next->addExpr(createAttribute(endAtom));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addGoto(const char * labelText)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(goto_stmt, curStmts);
    IHqlExpression * label= createVariable(labelText, makeBoolType());
    next->addExpr(label);
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addGroup()
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(group_stmt, curStmts);
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addGroupPass(IHqlExpression * pass)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(pass_stmt, curStmts);
    next->addExpr(LINK(pass));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addLabel(const char * labelText)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(label_stmt, curStmts);
    IHqlExpression * label= createVariable(labelText, makeBoolType());
    next->addExpr(label);
    return appendSimple(next);
}


IHqlStmt *  BuildCtx::addLine(const char * filename, unsigned lineNum)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlStmt(line_stmt, curStmts);
    if (filename)
    {
        next->addExpr(createConstant(filename));
        next->addExpr(createConstant(createIntValue(lineNum, sizeof(int), true)));
    }
    return appendSimple(next);
}

IHqlStmt * BuildCtx::addQuoted(const char * text)
{
    if (ignoreInput)
        return NULL;
    HqlStmt * next = new HqlQuoteStmt(quote_stmt, curStmts, text);
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addQuotedF(const char * format, ...)
{
    if (ignoreInput)
        return NULL;

    StringBuffer text;
    va_list args;
    va_start(args, format);
    text.valist_appendf(format, args);
    va_end(args);
    HqlStmt * next = new HqlQuoteStmt(quote_stmt, curStmts, text.str());
    return appendSimple(next);
}


IHqlStmt * BuildCtx::addQuotedCompound(const char * text, const char * extra)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlQuoteStmt(quote_compound_stmt, curStmts, text);
    if (extra)
        next->addExpr(createQuoted(extra, makeVoidType()));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addQuotedCompoundOpt(const char * text, const char * extra)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlQuoteStmt(quote_compoundopt_stmt, curStmts, text);
    if (extra)
        next->addExpr(createQuoted(extra, makeVoidType()));
    return appendCompound(next);
}


IHqlStmt * BuildCtx::addSwitch(IHqlExpression * source)
{
    if (ignoreInput)
        return NULL;
    HqlCompoundStmt * next = new HqlCompoundStmt(switch_stmt, curStmts);
    next->addExpr(LINK(source));
    return appendCompound(next);
}


HqlStmt * BuildCtx::appendCompound(HqlCompoundStmt * next)
{
    assertThrow(!ignoreInput);
    appendSimple(next);
    curStmts = &next->code;
    return next;
}

HqlStmt * BuildCtx::appendSimple(HqlStmt * next)                     
{
    assertThrow(!ignoreInput);
    if (nextPriority == OutermostScopePrio)
    {
        appendToOutermostScope(next);
    }
    else
    {
        next->setPriority(nextPriority);
        curStmts->appendStmt(*next);
    }
    nextPriority = curPriority;
    return next; 
}


void BuildCtx::appendToOutermostScope(HqlStmt * next)
{
    HqlStmts * searchStmts = curStmts;
    HqlStmts * insertStmts = NULL;
    HqlStmt * insertBefore = NULL;
    loop
    {
        HqlStmt * owner = searchStmts->owner;
        if (!owner)
            break;

        HqlStmts * ownerStmts = owner->queryContainer();
        switch (owner->getStmt())
        {
        case quote_compound_stmt:
        case quote_compoundopt_stmt:
        case indirect_stmt:
            goto found;
        case group_stmt:
            break;
        default:
            insertBefore = owner;
            insertStmts = ownerStmts;
            break;
        }

        searchStmts = ownerStmts;
    }

found:
    if (insertBefore)
    {
        next->setPriority(insertBefore->queryPriority());
        insertStmts->add(*next, insertStmts->find(*insertBefore));
    }
    else
    {
        next->setPriority(curPriority);
        curStmts->appendStmt(*next); 
    }
}


void BuildCtx::associate(HqlExprAssociation & next)
{
    assertex(next.represents->queryBody() == next.represents);
    if (!ignoreInput)
    {
        curStmts->appendOwn(OLINK(next));
    }
}


void BuildCtx::associateOwn(HqlExprAssociation & next)
{
    assertex(next.represents->queryBody() == next.represents);
    if (!ignoreInput)
    {
        curStmts->appendOwn(next);
    }
    else
        next.Release();             // can cause serious problems....
}


HqlExprAssociation *BuildCtx::associateExpr(IHqlExpression * represents, IHqlExpression * expr)
{
    if (!ignoreInput)
    {
        HqlExprAssociation * assoc = new HqlSimpleDefinedValue(represents->queryBody(), expr);
        curStmts->appendOwn(*assoc);
        return assoc;
    }
    return NULL;
}


HqlExprAssociation * BuildCtx::associateExpr(IHqlExpression * represents, const CHqlBoundExpr & bound)
{ 
    if (!ignoreInput)
    {
        HqlExprAssociation * assoc = new HqlBoundDefinedValue(represents->queryBody(), bound);
        curStmts->appendOwn(*assoc);
        return assoc;
    }
    return NULL;
}

IHqlExpression * BuildCtx::getTempDeclare(ITypeInfo * type, IHqlExpression * value)
{
    IHqlExpression * temp = createVariable(LINK(type));
    addDeclare(temp, value);
    return temp;
}

bool BuildCtx::hasAssociation(HqlExprAssociation & search, bool unconditional)
{
    HqlStmts * searchStmts = curStmts;
    loop
    {
        if (searchStmts->defs.contains(search))
            return true;
        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            return false;
        if (!unconditional)
        {
            switch (limitStmt->getStmt())
            {
            case filter_stmt:
                if (!matchesBoolean(limitStmt->queryExpr(0), true))
                    return false;
                break;
            case quote_compound_stmt:
            case quote_compoundopt_stmt:
            case switch_stmt:
            case case_stmt:
            case default_stmt:
            case loop_stmt:
                return false;
            }
        }
        searchStmts = limitStmt->queryContainer();
    }
}


void BuildCtx::walkAssociations(IAssociationVisitor & visitor)
{
    HqlStmts * searchStmts = curStmts;

    // walk all associations in the tree before this one
    loop
    {
        CIArrayOf<HqlExprAssociation> & defs = searchStmts->defs;
        ForEachItemInRev(idx, defs)
        {
            HqlExprAssociation & cur = defs.item(idx);
            if (visitor.visit(cur))
                return;
        }

        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            break;
        searchStmts = limitStmt->queryContainer();
    }
}


HqlExprAssociation * BuildCtx::queryAssociation(IHqlExpression * search, AssocKind kind, HqlExprCopyArray * selectors)
{
    HqlStmts * searchStmts = curStmts;

    if (!search)
        return NULL;
    search = search->queryBody();

    // search all statements in the tree before this one, to see
    // if an expression already exists...  If so return the target
    // of the assignment.
    loop
    {
        const CIArrayOf<HqlExprAssociation> & defs = searchStmts->defs;
#ifdef CACHE_DEFINITION_HASHES
        if (!selectors)
        {
            unsigned searchHash = getSearchHash(search);
            const DefinitionHashArray & hashes = searchStmts->exprHashes;
            ForEachItemInRev(idx, hashes)
            {
                if (hashes.item(idx) == searchHash)
                {
                    HqlExprAssociation & cur = defs.item(idx);
                    if (cur.represents == search)
                    {
                        if ((kind == AssocAny) || (cur.getKind() == kind))
                            return &cur;
                    }
                }
            }
        }
        else
#endif
        {
            ForEachItemInRev(idx, defs)
            {
                HqlExprAssociation & cur = defs.item(idx);
                IHqlExpression * represents = cur.represents.get();
                if (selectors && (cur.getKind() == AssocCursor))
                {
                    if (selectors->contains(*represents))
                        return NULL;
                }
                if (represents == search)
                    if ((kind == AssocAny) || (cur.getKind() == kind))
                        return &cur;
            }
        }

        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            break;
        searchStmts = limitStmt->queryContainer();
    }
    return NULL;
}


void BuildCtx::removeAssociation(HqlExprAssociation * search)
{
    if (!search)
        return;
    HqlStmts * searchStmts = curStmts;
    loop
    {
        bool matched = searchStmts->zap(*search);
        if (matched)
            return;
        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            break;
        searchStmts = limitStmt->queryContainer();
    }
        
    assertex(!"Association not found");
}


HqlExprAssociation * BuildCtx::queryFirstAssociation(AssocKind searchKind)
{
    HqlStmts * searchStmts = curStmts;

    // search all statements in the tree before this one, to see
    // if an expression already exists...  If so return the target
    // of the assignment.
    loop
    {
        CIArray & defs = searchStmts->defs;
        ForEachItemInRev(idx, defs)
        {
            HqlExprAssociation & cur = (HqlExprAssociation &)defs.item(idx);
            if (cur.getKind() == searchKind)
                return &cur;
        }

        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            break;
        searchStmts = limitStmt->queryContainer();
    }
    return NULL;
}


//Search for an association, but don't allow it to be conditional, or be hidden by the definition of a cursor.
HqlExprAssociation * BuildCtx::queryFirstCommonAssociation(AssocKind searchKind)
{
    HqlStmts * searchStmts = curStmts;

    // search all statements in the tree before this one, to see
    // if an expression already exists...  If so return the target
    // of the assignment.
    loop
    {
        CIArray & defs = searchStmts->defs;
        ForEachItemInRev(idx, defs)
        {
            HqlExprAssociation & cur = (HqlExprAssociation &)defs.item(idx);
            AssocKind kind = cur.getKind();
            if (kind == searchKind)
                return &cur;
            if (kind == AssocCursor)
                return NULL;
        }

        HqlStmt * limitStmt = searchStmts->queryStmt();
        if (!limitStmt)
            break;
        switch (limitStmt->getStmt())
        {
        //case quote_compound_stmt:
        //case quote_compoundopt_stmt,
        case filter_stmt:
        case label_stmt:
        case switch_stmt:
        case case_stmt:
        case default_stmt:
        case break_stmt:
        case continue_stmt:
            return NULL;
        }

        searchStmts = limitStmt->queryContainer();
    }
    return NULL;
}


HqlExprAssociation * BuildCtx::queryMatchExpr(IHqlExpression * search)
{
    HqlExprCopyArray selectors;
    search->gatherTablesUsed(NULL, &selectors);
    return queryAssociation(search, AssocExpr, selectors.ordinality() ? &selectors : NULL);
}


bool BuildCtx::getMatchExpr(IHqlExpression * expr, CHqlBoundExpr & tgt)
{
    HqlExprAssociation * match = queryMatchExpr(expr);

    if (match)
    {
        match->getBound(tgt);
        return true;
    }
    return false;
}

    

void BuildCtx::init(HqlStmts * _root)
{
    root = _root;
    curStmts = root;
    curPriority = NormalPrio;
    nextPriority = curPriority;
    ignoreInput = false;
}

bool BuildCtx::isChildOf(HqlStmt * stmt, HqlStmts * stmts)
{
    //MORE: Could improve by using depths
    do
    {
        if (stmts->find(*stmt) != NotFound)
            return true;
        stmt = stmt->queryContainer()->queryStmt();
    } while (stmt);
    return false;
}


void BuildCtx::selectCompound(IHqlStmt * stmt)
{
    HqlCompoundStmt & owner = (HqlCompoundStmt &)*stmt;
    curStmts = &owner.code;
}

void BuildCtx::selectContainer()
{
    HqlStmt * limitStmt = curStmts->queryStmt();
    assertex(limitStmt);
    curStmts = limitStmt->queryContainer();
    assertex(curStmts);
}


void BuildCtx::selectElse(IHqlStmt * stmt)
{
    //Ignoring input does not work, because code expects associations to be kept
    assertex(stmt);
    switch (stmt->getStmt())
    {
    case filter_stmt:
        assertThrow(stmt->numChildren() == 1);
        selectCompound(stmt);
        addBlock();
        break;
    default:
        throwUnexpected();
        break;
    }
}


unsigned BuildCtx::setPriority(unsigned newPrio)
{
    unsigned oldPriority = curPriority;
    if (!ignoreInput)
    {
        curPriority = newPrio;
        nextPriority = curPriority;
    }
    return oldPriority;
}

void BuildCtx::setNextPriority(unsigned newPrio)
{
    if (!ignoreInput)
        nextPriority = newPrio;
}



IHqlStmt * BuildCtx::recursiveGetBestContext(HqlStmts * searchStmts, HqlExprCopyArray & required)
{
    //Is it ok to move the expression before the owner statement?
    //First fail if any of the datasets that this expression is dependent on are defined in this scope.
    if (required.ordinality())
    {
        ForEachItemIn(i, searchStmts->defs)
        {
            HqlExprAssociation & cur = (HqlExprAssociation &)searchStmts->defs.item(i);
            if (required.contains(*cur.represents.get()))
                return NULL;
        }
    }

    HqlStmt * owner = searchStmts->owner;
    //Now check for poor places to hoist
    bool worthHoistingHere = false;
    switch (owner->getStmt())
    {
    case block_stmt:
    case group_stmt:
        break;
    case quote_compound_stmt:
    case quote_compoundopt_stmt:
    case indirect_stmt:
        return NULL;
    case filter_stmt:
    case switch_stmt:
        //MORE: Should make whether something ishoisted dependent on efficiency of the filter condition
        break;
    case loop_stmt:
        //MORE: Should make it dependent on the ordinality of the loop condition.
        worthHoistingHere = true;
        break;
    case case_stmt:
    case default_stmt:
        //Can't hoist here
        break;
    default:
        throwUnexpected();
    }

    HqlStmts * container = owner->queryContainer();
    if (container->owner)// && canHoistInParent)
    {
        IHqlStmt * match = recursiveGetBestContext(container, required);
        if (match)
            return match;
    }

    if (!worthHoistingHere)
        return NULL;

    //We've found somewhere we can insert the expression....
    unsigned existingPos = container->find(*owner);
    assertex(existingPos != NotFound);

    //insert a group just before the current statement
    HqlCompoundStmt * next = new HqlCompoundStmt(group_stmt, container);
    next->setPriority(owner->queryPriority());
    container->add(*next, existingPos);
    curStmts = &next->code;
    return next;
}


IHqlStmt * BuildCtx::replaceExpr(IHqlStmt * stmt, IHqlExpression * expr)
{
    //Highly dangerous - use with utmost care!
    assertex(stmt->getStmt() == expr_stmt);
    HqlStmt * castStmt = static_cast<HqlStmt *>(stmt);
    castStmt->killExprs();
    castStmt->addExpr(LINK(expr));
    return castStmt;
}

IHqlStmt * BuildCtx::selectBestContext(IHqlExpression * expr)
{
    if (containsTranslated(expr) || !curStmts->owner)
        return NULL;

    //MORE: Access to global context, context and other things...
    HqlExprCopyArray inScope;
    expr->gatherTablesUsed(NULL, &inScope);

    return recursiveGetBestContext(curStmts, inScope);
}

//---------------------------------------------------------------------------

HqlStmts::HqlStmts(HqlStmt * _owner)    : owner(_owner) 
{
}

void HqlStmts::appendStmt(HqlStmt & stmt)
{
    unsigned newPrio = stmt.queryPriority();
    unsigned right = ordinality();

    if (right == 0)
    {
        append(stmt);
    }
    else if (newPrio >= item(right-1).queryPriority())
    {
        while (item(right-1).isIncomplete())
        {
            if (newPrio > item(right-1).queryPriority())
                break;
            right--;
            if (right == 0)
                break;
        }
        add(stmt, right);
    }
    else
    {
        unsigned left = 0;
        while (right - left >= 2)
        {
            unsigned mid = (left + right - 1) / 2;
            HqlStmt & cur = item(mid);
            if (newPrio >= cur.queryPriority())
                left = mid+1;
            else
                right = mid+1;
        }

        if (newPrio >= item(left).queryPriority())
            ++left;
        while (left && item(left-1).isIncomplete() && (newPrio == item(left-1).queryPriority()))
            --left;
        add(stmt, left);
    }
}


//---------------------------------------------------------------------------

HqlStmt::HqlStmt(StmtKind _kind, HqlStmts * _container)
{
    kind = _kind;
    container = _container;
    incomplete = false;
    included = true;
}

void HqlStmt::addExpr(IHqlExpression * expr)
{
    //Only allocate a single extra expression at a time, since statements generally have very few (1) expressions
    exprs.ensure(exprs.ordinality()+1);
    exprs.append(*expr);
}

StmtKind HqlStmt::getStmt()
{
    return (StmtKind)kind;
}

StringBuffer & HqlStmt::getTextExtra(StringBuffer & out)
{
    return out;
}

static bool isEmptyGroup(IHqlStmt * stmt)
{
    if (!stmt)
        return true;
    switch (stmt->getStmt())
    {
    case group_stmt:
    case block_stmt:
        return stmt->numChildren() == 0;
    }
    return false;
}

bool HqlStmt::hasChildren() const
{
    if (numChildren() == 0)
        return false;

    unsigned count = numChildren();
    for (unsigned index = 0; index < count; index++)
    {
        IHqlStmt * cur = queryChild(index);
        if (cur->isIncluded())
            return true;
    }
    return false;
}
            
bool HqlStmt::isIncluded() const
{
    if (!included)
        return false;
    switch (kind)
    {
    case quote_compoundopt_stmt:
    case group_stmt:
        return hasChildren();
    case filter_stmt:
        if (isEmptyGroup(queryChild(0)) && isEmptyGroup(queryChild(1)))
            return false;
        break;
    }
    return true;
}

unsigned HqlStmt::numChildren() const
{
    return 0;
}

IHqlStmt * HqlStmt::queryChild(unsigned index) const
{
    return NULL;
}

HqlStmts * HqlStmt::queryContainer()
{
    return container;
}

IHqlExpression * HqlStmt::queryExpr(unsigned index)
{
    if (exprs.isItem(index))
        return &exprs.item(index);
    return NULL;
}


void HqlStmts::inheritDefinitions(HqlStmts & other)
{
    ForEachItemIn(i, other.defs)
    {
        defs.append(OLINK(other.defs.item(i)));
#ifdef CACHE_DEFINITION_HASHES
        exprHashes.append(other.exprHashes.item(i));
#endif
    }

}

//---------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning( disable : 4355 ) // 'this' : used in base member initializer list
#endif
HqlCompoundStmt::HqlCompoundStmt(StmtKind _kind, HqlStmts * _container) : HqlStmt(_kind, _container), code(this)
{
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif


void HqlCompoundStmt::mergeScopeWithContainer()
{
    container->inheritDefinitions(code);
}

unsigned HqlCompoundStmt::numChildren() const
{
    return code.ordinality();
}

IHqlStmt * HqlCompoundStmt::queryChild(unsigned index) const
{
    if (code.isItem(index))
        return &code.item(index);
    return NULL;
}


//---------------------------------------------------------------------------

StringBuffer & HqlQuoteStmt::getTextExtra(StringBuffer & out)
{
    return out.append(text);
}


//---------------------------------------------------------------------------
int queryMemsetChar(IHqlExpression * expr)
{
    if (expr->getOperator() != no_constant)
        return -1;
    unsigned size = expr->queryType()->getSize();
    if (size == 0)
        return -1;
    const byte * data = (const byte *)expr->queryValue()->queryValue();
    byte match = data[0];
    while (--size)
        if (*++data != match)
            return -1;
    return match;
}


static IHqlExpression * extractNonConstant(IHqlExpression * expr, unsigned & delta)
{
    switch (expr->getOperator())
    {
    case no_constant:
        delta += (unsigned)getIntValue(expr);
        return NULL;
    case no_add:
        {
            IHqlExpression * left = expr->queryChild(0);
            OwnedHqlExpr newLeft = extractNonConstant(left, delta);
            IHqlExpression * right = expr->queryChild(1);
            OwnedHqlExpr newRight = extractNonConstant(right, delta);
            if (!newLeft) return newRight.getClear();
            if (!newRight) return newLeft.getClear();
            if ((left == newLeft) && (right == newRight))
                return LINK(expr);
            return createValue(no_add, expr->getType(), newLeft.getClear(), newRight.getClear());
        }
    }
    return LINK(expr);
}

IHqlExpression * peepholeAddExpr(IHqlExpression * left, IHqlExpression * right)
{
    unsigned delta = 0;
    IHqlExpression * simpleLeft = extractNonConstant(left, delta);
    IHqlExpression * simpleRight = extractNonConstant(right, delta);
    IHqlExpression * ret;
    if (simpleLeft)
    {
        if (simpleRight)
            ret = createValue(no_add, left->getType(), simpleLeft, simpleRight);
        else
            ret = simpleLeft;
    }
    else
        ret = simpleRight;

    if (!delta && ret)
        return ret;
    IHqlExpression * value = getSizetConstant(delta);
    if (!ret)
        return value;
    return createValue(no_add, left->getType(), ret, value);
    if ((left->getOperator() == no_constant) && (right->getOperator() == no_constant))
        return getSizetConstant((size32_t)left->queryValue()->getIntValue() + (size32_t)right->queryValue()->getIntValue());
    return createValue(no_add, left->getType(), LINK(left), LINK(right));
}

bool rightFollowsLeft(IHqlExpression * left, IHqlExpression * leftLen, IHqlExpression * right)
{
    OwnedHqlExpr sum = peepholeAddExpr(left, leftLen);
    if (sum == right)
        return true;

    if (left->getOperator() != right->getOperator())
    {
        if (right->getOperator() == no_add)
        {
            if ((left == right->queryChild(0)) && (leftLen == right->queryChild(1)))
                return true;
            if ((left == right->queryChild(1)) && (leftLen == right->queryChild(0)))
                return true;
        }
        return false;
    }

    switch (left->getOperator())
    {
    case no_constant:
        {
            ITypeInfo * leftType = left->queryType();
            ITypeInfo * rightType = right->queryType();
            if (leftType->getTypeCode() != rightType->getTypeCode())
                return false;
            switch (leftType->getTypeCode())
            {
            case type_int:
                if (leftLen->getOperator() != no_constant)
                    return false;
                if (left->queryValue()->getIntValue() + leftLen->queryValue()->getIntValue() == right->queryValue()->getIntValue())
                    return true;
                return false;
            }
            return false;
        }
    case no_add:
        if ((left == right->queryChild(0)) && (leftLen == right->queryChild(1)))
            return true;
        if (left->queryChild(1) == right->queryChild(1))
        {
            if (rightFollowsLeft(left->queryChild(0), leftLen, right->queryChild(0)))
                return true;
        }
        //fall through
    case no_index:
        if (left->queryChild(0) != right->queryChild(0))
            return false;
        return rightFollowsLeft(left->queryChild(1), leftLen, right->queryChild(1));
    }

    unsigned numLeft = left->numChildren();
    if (numLeft == 0 || (numLeft != right->numChildren()))
        return false;
    ForEachChild(idx, left)
    {
        if (!rightFollowsLeft(left->queryChild(idx), leftLen, right->queryChild(idx)))
            return false;
    }
    return true;
}

static IHqlExpression * createDataForMemset(unsigned size, byte value)
{
    if (size < 100)
    {
        void * temp = alloca(size);
        memset(temp, value, size);
        return createConstant(createDataValue((char *)temp, size));
    }
    void * temp = malloc(size);
    memset(temp, value, size);
    IHqlExpression * ret = createConstant(createDataValue((char *)temp, size));
    free(temp);
    return ret;
}


static IHqlExpression * createDataForIntegerZero(unsigned size)
{
    return createDataForMemset(size, 0);
}

class SpecialFunction
{
public:
    SpecialFunction() { wasAssign = false; name = NULL; }

    HqlStmt * createStmt(HqlStmts & curStmts, HqlCppTranslator & translator);

    bool canOptimize() const;
    void expandValue(void * target) const;
    bool extractIsSpecial(IHqlStmt & stmt, bool memsetOnly, unsigned peepholeOptions);
    bool isBigClear() const;
    int queryClearValue() const;
    bool queryCombine(const SpecialFunction & next, bool memsetOnly, size32_t combineStringLimit);

private:
    _ATOM name;
    HqlExprAttr src;
    HqlExprAttr tgt;
    HqlExprAttr srcLen;
    HqlExprAttr tgtLen;
    bool wasAssign;
};

//Always convert rtlWriteInt(rtlReadInt()) the inline memcpy is going to be much better.
//It should probably be converted earlier....
bool isAwkwardIntSize(IHqlExpression * size)
{
    IValue * value = size->queryValue();
    if (value)
    {
        switch (value->getIntValue())
        {
        case 3:
        case 5:
        case 6:
        case 7:
            return true;
        }
    }
    return false;
}


bool SpecialFunction::canOptimize() const
{
    if ((name == memcpyAtom) && (queryMemsetChar(src) >= 0))
    {
        if ((getIntValue(srcLen, 0) > 1) || !wasAssign)
            return true;
    }
    if ((name == memcpyAtom) && isAwkwardIntSize(srcLen))
        return true;

    return false;
}

HqlStmt * SpecialFunction::createStmt(HqlStmts & curStmts, HqlCppTranslator & translator)
{
    HqlExprArray args;

    _ATOM func = name;
    if (name == memsetAtom)
    {
        func = memsetAtom;
        args.append(*LINK(tgt));
        args.append(*LINK(src));
        args.append(*LINK(srcLen));
    }
    else if (name == memcpyAtom)
    {
        int clearByte = queryMemsetChar(src);
        size32_t size = (size32_t)getIntValue(srcLen, 0);
        if (clearByte == 0)
        {
            //if length is 1,2,4 then use an assignment instead.
            switch (size)
            {
            case 1:
            case 2:
            case 4:
            case 8:
                {
                    OwnedITypeInfo type = makeIntType(size, false);
                    OwnedHqlExpr castTgt = createValue(no_cast, makePointerType(LINK(type)), LINK(tgt));
                    OwnedHqlExpr deref = createValue(no_deref, makeReferenceModifier(LINK(type)), LINK(castTgt));
                    OwnedHqlExpr src = createConstant(type->castFrom(true, (__int64)0));
                    HqlStmt * next = new HqlStmt(assign_stmt, &curStmts);
                    next->addExpr(LINK(deref));
                    next->addExpr(LINK(src));
                    return next;
                }
            }
        }
        //MORE: assignment of 1,2,4 bytes possibly better as an assign?
        if (clearByte >= 0)
        {
            func = memsetAtom;
            args.append(*LINK(tgt));
            args.append(*createConstant(createIntValue(clearByte, sizeof(int), true)));
            args.append(*LINK(srcLen));
        }
        else
        {
            args.append(*LINK(tgt));
            args.append(*LINK(src));
            args.append(*LINK(srcLen));
        }
    }
    else if (name == deserializerReadNAtom || name == serializerPutAtom)
    {
        args.append(*LINK(src));
        args.append(*LINK(tgtLen));
        args.append(*LINK(tgt));
    }
    else if ((name == ebcdic2asciiAtom) || (name == ascii2ebcdicAtom))
    {
        args.append(*LINK(tgtLen));
        args.append(*LINK(tgt));
        args.append(*LINK(srcLen));
        args.append(*LINK(src));
    }
    else if (name == deserializerSkipNAtom)
    {
        args.append(*LINK(src));
        args.append(*LINK(srcLen));
    }
    else
        UNIMPLEMENTED;

    HqlStmt * next = new HqlStmt(expr_stmt, &curStmts);
    next->addExpr(translator.bindTranslatedFunctionCall(func, args));
    return next;
}

IHqlExpression * stripTranslatedCasts(IHqlExpression * e)
{
    loop
    {
        switch (e->getOperator())
        {
        case no_cast:
        case no_implicitcast:
        case no_typetransfer:
            {
                IHqlExpression * child = e->queryChild(0);
                if (hasWrapperModifier(child->queryType()))
                    return e;
                e = child;
                break;
            }
        default:
            return e;
        }
    }
}


void SpecialFunction::expandValue(void * target) const
{
    size32_t size = (size32_t)getIntValue(srcLen);
    if (name == memsetAtom)
        memset(target, (int)getIntValue(src), size);
    else
        memcpy(target, src->queryValue()->queryValue(), size);
}


bool SpecialFunction::extractIsSpecial(IHqlStmt & stmt, bool memsetOnly, unsigned peepholeOptions)
{
    if (stmt.getStmt() == expr_stmt)
    {
        IHqlExpression * expr = stmt.queryExpr(0);
        if (expr->getOperator() != no_externalcall)
            return false;
        name = expr->queryName();
        if (name == memcpyAtom)
        {
            src.set(stripTranslatedCasts(expr->queryChild(1)));
            if (memsetOnly && (queryMemsetChar(src) == -1))
                return false;
            tgt.set(stripTranslatedCasts(expr->queryChild(0)));
            srcLen.set(expr->queryChild(2));
            tgtLen.set(srcLen);
            return true;
        }
        if (name == deserializerReadNAtom || name == serializerPutAtom)
        {
            if (memsetOnly)
                return false;
            src.set(expr->queryChild(0));
            tgt.set(stripTranslatedCasts(expr->queryChild(2)));
            tgtLen.set(expr->queryChild(1));
            return true;
        }
        if ((name == ebcdic2asciiAtom) || (name == ascii2ebcdicAtom))
        {
            if (memsetOnly)
                return false;
            src.set(expr->queryChild(3));
            tgt.set(expr->queryChild(1));
            srcLen.set(expr->queryChild(2));
            tgtLen.set(expr->queryChild(0));
            return srcLen == tgtLen;
        }
        if (name == memsetAtom)
        {
            IHqlExpression * value = expr->queryChild(1);
            IHqlExpression * len = expr->queryChild(2);
            if (len->queryValue() && value->queryValue())
            {
                tgt.set(stripTranslatedCasts(expr->queryChild(0)));
                src.set(value);
                srcLen.set(len);
                tgtLen.set(srcLen);
            }
            return true;
        }
        if (name == deserializerSkipNAtom)
        {
            src.set(expr->queryChild(0));
            srcLen.set(expr->queryChild(1));
            return true;
        }
        unsigned size = 0;
        if (name == writeIntAtom[3])
            size = 3;
        else if (name == writeIntAtom[5])
            size = 5;
        else if (name == writeIntAtom[6])
            size = 6;
        else if (name == writeIntAtom[7])
            size = 7;
        if (size)
        {
            IHqlExpression * value = expr->queryChild(1);
            if (isZero(value))
            {
                name = memcpyAtom;
                src.setown(createDataForIntegerZero(size));
                tgt.set(stripTranslatedCasts(expr->queryChild(0)));
                srcLen.setown(getSizetConstant(size));
                tgtLen.set(srcLen);
                return true;
            }
            if (memsetOnly)
                return false;

            while (value->getOperator() == no_typetransfer)
                value = value->queryChild(0);
            if (value->getOperator() == no_externalcall)
            {
                if ((value->queryName() == readIntAtom[size][true]) ||
                    (value->queryName() == readIntAtom[size][false]))
                {
                    name = memcpyAtom;
                    src.set(stripTranslatedCasts(value->queryChild(0)));
                    tgt.set(stripTranslatedCasts(expr->queryChild(0)));
                    srcLen.setown(getSizetConstant(size));
                    tgtLen.set(srcLen);
                    return true;
                }
            }
        }
        return false;
    }
    else if (stmt.getStmt() == assign_stmt)
    {
        wasAssign = true;
        tgt.set(stmt.queryExpr(0));
        src.set(stmt.queryExpr(1));
        ITypeInfo * tgtType = tgt->queryType();
        ITypeInfo * srcType = src->queryType();
        if (!isSameBasicType(srcType, tgtType))
        {
            if (!isSameFullyUnqualifiedType(srcType, tgtType))
                return false;
        }
        while (tgt->getOperator() == no_typetransfer)
            tgt.set(tgt->queryChild(0));
        if (tgt->getOperator() != no_deref)
            return false;
        while (src->getOperator() == no_typetransfer)
            src.set(src->queryChild(0));

        size32_t targetSize = tgtType->getSize();
        if (src->getOperator() != no_deref)
        {
            type_t tc = tgtType->getTypeCode();

            OwnedHqlExpr newSrcExpr;
            switch (tc)
            {
            case type_pointer:
            case type_table:
            //case type_row:
                if (src->getOperator() == no_nullptr)
                {
                    void * ptr = NULL;
                    targetSize = sizeof(void *);
                    newSrcExpr.setown(createConstant(createDataValue((const char *)&ptr, targetSize)));
                }
                break;
            }
            
            if (!memsetOnly)
            {
                switch (tc)
                {
                case type_int:
                case type_swapint:
                case type_boolean:
                    if (src->getOperator() == no_constant)
                    {
                        OwnedHqlExpr cast = ensureExprType(src, tgtType);
                        IValue * castValue = cast->queryValue();
                        if (castValue)
                        {
                            void * temp = alloca(targetSize);
                            castValue->toMem(temp);
                            newSrcExpr.setown(createConstant(createDataValue((const char *)temp, targetSize)));
                        }
                    }
                    break;
                case type_real:
                    if ((peepholeOptions & PHOconvertReal) && (src->getOperator() == no_constant))
                    {
                        OwnedHqlExpr cast = ensureExprType(src, tgtType);
                        IValue * castValue = cast->queryValue();
                        if (castValue)
                        {
                            void * temp = alloca(targetSize);
                            castValue->toMem(temp);
                            newSrcExpr.setown(createConstant(createDataValue((const char *)temp, targetSize)));
                        }
                    }
                    break;
                }
            }

            if (!newSrcExpr && isZero(src))
                newSrcExpr.setown(createDataForIntegerZero(targetSize));

            if (!newSrcExpr)
                return false;
            src.set(newSrcExpr);
        }
        else
        {
            if (memsetOnly)
                return false;
            src.set(src->queryChild(0));
        }
        srcLen.setown(getSizetConstant(targetSize));
        tgtLen.set(srcLen);
        tgt.set(stripTranslatedCasts(tgt->queryChild(0)));
        src.set(stripTranslatedCasts(src));
        name = memcpyAtom;
        return true;
    }
    return false;
}

bool SpecialFunction::isBigClear() const
{
    if ((name == memsetAtom) || (name == memcpyAtom))
        return getIntValue(srcLen, 0) > CLEAR_COPY_THRESHOLD;
    return false;
}

int SpecialFunction::queryClearValue() const
{
    if (name == memcpyAtom) 
        return queryMemsetChar(src);
    if (name == memsetAtom)
        return (int)getIntValue(src, -1);
    return -1;
}

bool SpecialFunction::queryCombine(const SpecialFunction & next, bool memsetOnly, size32_t combineStringLimit)
{
    if (name != next.name)
    {
        if (!((name == memsetAtom) && (next.name == memcpyAtom)) && 
            !((name == memcpyAtom) && (next.name == memsetAtom)))
            return false;
    }
    if (name == deserializerSkipNAtom)
    {
        if (src != next.src)
            return false;
        srcLen.setown(peepholeAddExpr(srcLen, next.srcLen));
        return true;
    }
    if (rightFollowsLeft(tgt, tgtLen, next.tgt))
    {
        if ((name != memsetAtom) && (next.name != memsetAtom))
        {
            if (name == deserializerReadNAtom || name == serializerPutAtom)
            {
                tgtLen.setown(peepholeAddExpr(tgtLen, next.tgtLen));
                return true;
            }

            if (rightFollowsLeft(src, srcLen, next.src))
            {
                srcLen.setown(peepholeAddExpr(srcLen, next.srcLen));
                tgtLen.setown(peepholeAddExpr(tgtLen, next.tgtLen));
                return true;
            }
        }
        IValue * srcValue = src->queryValue();
        IValue * nextValue = next.src->queryValue();
        if (srcValue && nextValue)
        {
            //Don't combine things that can be converted to different memsets.
            //This would work better if we first combined items that could be done as memsets and then combined strings
            //processing in order we hit them doesn't work as well.
            int clearValue = queryClearValue();
            int nextClearValue = next.queryClearValue();
            if (memsetOnly)
            {
                assertex((clearValue != -1) && (nextClearValue != -1));
                if (clearValue != nextClearValue)
                    return false;
            }

            if ((isBigClear() && (clearValue != -1)) || (next.isBigClear() && (nextClearValue != -1)))
            {
                if (clearValue != nextClearValue)
                    return false;
            }

            size32_t curSize = (size32_t)getIntValue(srcLen);
            size32_t nextSize = (size32_t)getIntValue(next.srcLen);
            if (curSize + nextSize > combineStringLimit)
                return false;

            byte * temp = (byte *)malloc(curSize + nextSize);
            expandValue(temp);
            next.expandValue(temp+curSize);
            src.setown(createConstant(createDataValue((const char *)temp, curSize + nextSize)));
            free(temp);
            srcLen.setown(getSizetConstant(curSize + nextSize));
            tgtLen.set(srcLen);
            if (name == memsetAtom)
                name = memcpyAtom;
            return true;
        }
    }
    return false;
}

PeepHoleOptimizer::PeepHoleOptimizer(HqlCppTranslator & _translator) : translator(_translator) 
{ 
    combineStringLimit = -1;
    peepholeOptions = 0;
    if (translator.queryOptions().convertRealAssignToMemcpy)
        peepholeOptions |= PHOconvertReal;
    switch (translator.queryOptions().targetCompiler)
    {
    case Vs6CppCompiler:
        combineStringLimit = 32000;
        break;
    }
}

void PeepHoleOptimizer::optimize(HqlStmts & stmts)
{
    unsigned max = stmts.ordinality();      // can change as processing proceeds.
    SpecialFunction prevMatch;
    for (unsigned pass=0; pass < 2; pass++)
    {
        bool memsetOnly = (pass == 0);
        for (unsigned i =0; i < max; i++)
        {
            IHqlStmt & cur = stmts.item(i);
            if (prevMatch.extractIsSpecial(cur, memsetOnly, peepholeOptions))
            {
                unsigned j = i+1;
                unsigned prevMax = max;
                SpecialFunction nextMatch;
                while (j < max)
                {
                    IHqlStmt & next = stmts.item(j);
                    if (!nextMatch.extractIsSpecial(next, memsetOnly, peepholeOptions))
                        break;
                    if (!prevMatch.queryCombine(nextMatch, memsetOnly, combineStringLimit))
                        break;

                    stmts.remove(i);
                    max--;
                }

                if (max != prevMax || prevMatch.canOptimize())
                {
                    stmts.remove(i);
                    stmts.add(*prevMatch.createStmt(stmts, translator), i);
                }
            }
        }
    }

    for (unsigned i2=0; i2 < max; i2++)
    {
        IHqlStmt & cur = stmts.item(i2);
        if (cur.numChildren() != 0)
            optimize(((HqlCompoundStmt&)cur).code);
    }

}



void peepholeOptimize(HqlCppInstance & instance, HqlCppTranslator & translator)
{
    PeepHoleOptimizer optimizer(translator);
    ForEachItemIn(idx, instance.sections)
        optimizer.optimize(((HqlCppSection&)instance.sections.item(idx)).stmts);
}


//---------------------------------------------------------------------------

AssociationIterator::AssociationIterator(BuildCtx & ctx)
{
    rootStmts = ctx.curStmts;
    curStmts = NULL;
    curIdx = 0;
}


bool AssociationIterator::first()
{
    curStmts = rootStmts;
    curIdx = curStmts->defs.ordinality();
    return doNext();
}

bool AssociationIterator::doNext()
{
    loop
    {
        if (curIdx-- != 0)
            return true;

        HqlStmt * limitStmt = curStmts->queryStmt();
        if (!limitStmt)
        {
            curStmts = NULL;
            return false;
        }
        curStmts = limitStmt->queryContainer();
        curIdx = curStmts->defs.ordinality();
    }
}

HqlExprAssociation & AssociationIterator::get()
{
    CIArray & defs = curStmts->defs;
    return (HqlExprAssociation &)defs.item(curIdx);
}


//---------------------------------------------------------------------------

bool FilteredAssociationIterator::doNext()
{
    while (AssociationIterator::doNext())
    {
        HqlExprAssociation & cur = AssociationIterator::get();
        if (cur.getKind() == searchKind)
            return true;
    }

    return false;
}

bool RowAssociationIterator::doNext()
{
    while (AssociationIterator::doNext())
    {
        HqlExprAssociation & cur = AssociationIterator::get();
        if (cur.isRowAssociation())
            return true;
    }

    return false;
};


unsigned calcTotalChildren(IHqlStmt * stmt)
{
    if (!stmt->isIncluded())
        return 0;
    unsigned num = stmt->numChildren();
    unsigned total = 1;
    switch (stmt->getStmt())
    {
        case alias_stmt:
        case group_stmt:
        case pass_stmt:
        case indirect_stmt:
            total = 0;
            break;
    }

    for (unsigned i=0; i < num; i++)
        total += calcTotalChildren(stmt->queryChild(i));
    return total;
}

