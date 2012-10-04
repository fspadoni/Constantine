// Copyright 2012 by Laszlo Nagy [see file MIT-LICENSE]

#include "ModuleAnalysis.hpp"
#include "ScopeAnalysis.hpp"

#include <iterator>
#include <map>

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/range.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/transform.hpp>


namespace {

// Report function for pseudo constness analysis.
void ReportVariablePseudoConstness(clang::DiagnosticsEngine & DE, clang::DeclaratorDecl const * const V) {
    static char const * const Message = "variable '%0' could be declared as const";
    unsigned const Id = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, Message);
    clang::DiagnosticBuilder DB = DE.Report(V->getLocStart(), Id);
    DB << V->getNameAsString();
}

// Report function for debug functionality.
void ReportVariableDeclaration(clang::DiagnosticsEngine & DE, clang::DeclaratorDecl const * const V) {
    static char const * const Message = "variable '%0' declared here";
    unsigned const Id = DE.getCustomDiagID(clang::DiagnosticsEngine::Note, Message);
    clang::DiagnosticBuilder DB = DE.Report(V->getLocStart(), Id);
    DB << V->getNameAsString();
    DB.setForceEmit();
}

void ReportFunctionDeclaration(clang::DiagnosticsEngine & DE, clang::FunctionDecl const * const F) {
    static char const * const Message = "function '%0' declared here";
    unsigned const Id = DE.getCustomDiagID(clang::DiagnosticsEngine::Note, Message);
    clang::DiagnosticBuilder DB = DE.Report(F->getSourceRange().getBegin(), Id);
    DB << F->getNameAsString();
    DB.setForceEmit();
}


typedef std::set<clang::DeclaratorDecl const *> Variables;


// Pseudo constness analysis detects what variable can be declare as const.
// This analysis runs through multiple scopes. We need to store the state of
// the ongoing analysis. Once the variable was changed can't be const.
class PseudoConstnessAnalysisState : public boost::noncopyable {
public:
    PseudoConstnessAnalysisState()
        : boost::noncopyable()
        , Candidates()
        , Changed()
    { }

    void Eval(ScopeAnalysis const & Analysis, clang::DeclaratorDecl const * const V) {
        if (Analysis.WasChanged(V)) {
            Candidates.erase(V);
            Changed.insert(V);
        } else if (Analysis.WasReferenced(V)) {
            if (Changed.end() == Changed.find(V)) {
                if (! IsConst(*V)) {
                    Candidates.insert(V);
                }
            }
        }
    }

    void GenerateReports(clang::DiagnosticsEngine & DE) {
        boost::for_each(Candidates,
            boost::bind(ReportVariablePseudoConstness, boost::ref(DE), _1));
    }

private:
    static bool IsConst(clang::DeclaratorDecl const & D) {
        return (D.getType().getNonReferenceType().isConstQualified());
    }

private:
    Variables Candidates;
    Variables Changed;
};


// Represents a function declaration. This class try to add new methods
// to clang::FunctionDecl. That type is part of an inheritance tree.
// To add functions to it, here is a duplication of the hierarchy.
struct FunctionWrapper : public boost::noncopyable {
protected:
    virtual clang::FunctionDecl const * GetFunctionDecl() const = 0;

    virtual Variables GetVariables() const = 0;

public:
    // Debug functionality
    virtual void DumpFuncionDeclaration(clang::DiagnosticsEngine &) const;
    virtual void DumpVariableDeclaration(clang::DiagnosticsEngine &) const;

    // Analysis functionality
    virtual void DumpVariableChanges(clang::DiagnosticsEngine &) const;
    virtual void DumpVariableUsages(clang::DiagnosticsEngine &) const;
    virtual void CheckPseudoConstness(PseudoConstnessAnalysisState &) const;
};

void FunctionWrapper::DumpFuncionDeclaration(clang::DiagnosticsEngine & DE) const {
    clang::FunctionDecl const * F = GetFunctionDecl();
    ReportFunctionDeclaration(DE, F);
}

void FunctionWrapper::DumpVariableDeclaration(clang::DiagnosticsEngine & DE) const {
    boost::for_each(GetVariables(),
        boost::bind(ReportVariableDeclaration, boost::ref(DE), _1));
}

void FunctionWrapper::DumpVariableChanges(clang::DiagnosticsEngine & DE) const {
    clang::FunctionDecl const * F = GetFunctionDecl();
    ScopeAnalysis const & Analysis = ScopeAnalysis::AnalyseThis(*(F->getBody()));
    Analysis.DebugChanged(DE);
}

void FunctionWrapper::DumpVariableUsages(clang::DiagnosticsEngine & DE) const {
    clang::FunctionDecl const * F = GetFunctionDecl();
    ScopeAnalysis const & Analysis = ScopeAnalysis::AnalyseThis(*(F->getBody()));
    Analysis.DebugReferenced(DE);
}

void FunctionWrapper::CheckPseudoConstness(PseudoConstnessAnalysisState & State) const {
    clang::FunctionDecl const * F = GetFunctionDecl();
    ScopeAnalysis const & Analysis = ScopeAnalysis::AnalyseThis(*(F->getBody()));

    boost::for_each(GetVariables(),
        boost::bind(&PseudoConstnessAnalysisState::Eval, &State, boost::cref(Analysis), _1));
}

Variables GetVariablesFromContext(clang::DeclContext const * const F) {
    Variables Result;
    for (clang::DeclContext::decl_iterator It(F->decls_begin()), End(F->decls_end()); It != End; ++It ) {
        if (clang::VarDecl const * const D = clang::dyn_cast<clang::VarDecl const>(*It)) {
            Result.insert(D);
        }
    }
    return Result;
}

Variables GetVariablesFromRecord(clang::RecordDecl const * const F) {
    Variables Result;
    for (clang::RecordDecl::field_iterator It(F->field_begin()), End(F->field_end()); It != End; ++It ) {
        if (clang::FieldDecl const * const D = clang::dyn_cast<clang::FieldDecl const>(*It)) {
            Result.insert(D);
        }
    }
    return Result;
}

// Implement wrapper for simple C functions.
class FunctionDeclWrapper : public FunctionWrapper {
public:
    FunctionDeclWrapper(clang::FunctionDecl const * const F)
        : FunctionWrapper()
        , Function(F)
    { }

protected:
    clang::FunctionDecl const * GetFunctionDecl() const {
        return Function;
    }

    Variables GetVariables() const {
        return GetVariablesFromContext(Function);
    }

private:
    clang::FunctionDecl const * const Function;
};

// Implement wrapper for C++ methods.
class MethodDeclWrapper : public FunctionWrapper {
public:
    MethodDeclWrapper(clang::CXXMethodDecl const * const F)
        : FunctionWrapper()
        , Function(F)
    { }

protected:
    clang::FunctionDecl const * GetFunctionDecl() const {
        return Function;
    }

    Variables GetVariables() const {
        Variables Result;
        boost::copy(GetVariablesFromContext(Function)
            , std::insert_iterator<Variables>(Result, Result.begin()));
        boost::copy(GetVariablesFromRecord(Function->getParent()->getCanonicalDecl())
            , std::insert_iterator<Variables>(Result, Result.begin()));
        return Result;
    }

private:
    clang::CXXMethodDecl const * const Function;
};


// This class collect function declarations and create wrapped classes around them.
class FunctionCollector : public clang::RecursiveASTVisitor<FunctionCollector> {
public:
    void DumpFuncionDeclaration(clang::DiagnosticsEngine & DE) const {
        boost::for_each(Functions | boost::adaptors::map_values,
            boost::bind(&FunctionWrapper::DumpFuncionDeclaration, _1, boost::ref(DE)));
    }

    void DumpVariableDeclaration(clang::DiagnosticsEngine & DE) const {
        boost::for_each(Functions | boost::adaptors::map_values,
            boost::bind(&FunctionWrapper::DumpVariableDeclaration, _1, boost::ref(DE)));
    }

    void DumpVariableChanges(clang::DiagnosticsEngine & DE) const {
        boost::for_each(Functions | boost::adaptors::map_values,
            boost::bind(&FunctionWrapper::DumpVariableChanges, _1, boost::ref(DE)));
    }

    void DumpVariableUsages(clang::DiagnosticsEngine & DE) const {
        boost::for_each(Functions | boost::adaptors::map_values,
            boost::bind(&FunctionWrapper::DumpVariableUsages, _1, boost::ref(DE)));
    }

    void DumpPseudoConstness(clang::DiagnosticsEngine & DE) const {
        PseudoConstnessAnalysisState State;
        boost::for_each(Functions | boost::adaptors::map_values,
            boost::bind(&FunctionWrapper::CheckPseudoConstness, _1, boost::ref(State)));
        State.GenerateReports(DE);
    }

    // public visitor method.
    bool VisitFunctionDecl(clang::FunctionDecl const * F) {
        clang::FunctionDecl const * const CD = F->getCanonicalDecl();
        if (F->isThisDeclarationADefinition()) {
            Functions[CD] = FunctionWrapperPtr(new FunctionDeclWrapper(F));
        }
        return true;
    }

    bool VisitCXXMethodDecl(clang::CXXMethodDecl const * F) {
        clang::CXXMethodDecl const * const CD = F->getCanonicalDecl();
        if (F->isThisDeclarationADefinition()) {
            Functions[CD] = FunctionWrapperPtr(new MethodDeclWrapper(F));
        }
        return true;
    }

private:
    typedef boost::shared_ptr<FunctionWrapper> FunctionWrapperPtr;
    typedef std::map<clang::FunctionDecl const *, FunctionWrapperPtr> FunctionWrapperPtrs;

    FunctionWrapperPtrs Functions;
};

} // namespace anonymous


ModuleAnalysis::ModuleAnalysis(clang::CompilerInstance const & Compiler, Target T)
    : boost::noncopyable()
    , clang::ASTConsumer()
    , Reporter(Compiler.getDiagnostics())
    , State(T)
{ }

void ModuleAnalysis::HandleTranslationUnit(clang::ASTContext & Ctx) {
    FunctionCollector Collector;
    Collector.TraverseDecl(Ctx.getTranslationUnitDecl());

    switch (State) {
    case FuncionDeclaration :
        Collector.DumpFuncionDeclaration(Reporter);
        break;
    case VariableDeclaration :
        Collector.DumpVariableDeclaration(Reporter);
        break;
    case VariableChanges :
        Collector.DumpVariableChanges(Reporter);
        break;
    case VariableUsages :
        Collector.DumpVariableUsages(Reporter);
        break;
    case PseudoConstness :
        Collector.DumpPseudoConstness(Reporter);
        break;
    }
}
