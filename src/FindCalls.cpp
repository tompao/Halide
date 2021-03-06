#include "FindCalls.h"

namespace Halide{
namespace Internal {

using std::map;
using std::vector;
using std::string;

/* Find all the internal halide calls in an expr */
class FindCalls : public IRVisitor {
public:
    map<string, Function> calls;

    using IRVisitor::visit;

    void include_function(Function f) {
        map<string, Function>::iterator iter = calls.find(f.name());
        if (iter == calls.end()) {
            calls[f.name()] = f;
        } else {
            assert(iter->second.same_as(f) &&
                   "Can't compile a pipeline using multiple functions with same name");
        }
    }

    void visit(const Call *call) {
        IRVisitor::visit(call);

        if (call->call_type == Call::Halide) {
            Function f = call->func;
            include_function(f);
        }

    }
};

void populate_environment(Function f, map<string, Function> &env, bool recursive = true) {
    map<string, Function>::const_iterator iter = env.find(f.name());
    if (iter != env.end()) {
        assert(iter->second.same_as(f) &&
               "Can't compile a pipeline using multiple functions with same name");
        return;
    }

    FindCalls calls;
    for (size_t i = 0; i < f.values().size(); i++) {
        f.values()[i].accept(&calls);
    }

    // Consider reductions
    for (size_t j = 0; j < f.reductions().size(); j++) {
        ReductionDefinition r = f.reductions()[j];
        for (size_t i = 0; i < r.values.size(); i++) {
            r.values[i].accept(&calls);
        }
        for (size_t i = 0; i < r.args.size(); i++) {
            r.args[i].accept(&calls);
        }

        ReductionDomain d = r.domain;
        if (r.domain.defined()) {
            for (size_t i = 0; i < d.domain().size(); i++) {
                d.domain()[i].min.accept(&calls);
                d.domain()[i].extent.accept(&calls);
            }
        }
    }

    // Consider extern calls
    if (f.has_extern_definition()) {
        for (size_t i = 0; i < f.extern_arguments().size(); i++) {
            ExternFuncArgument arg = f.extern_arguments()[i];
            if (arg.is_func()) {
                Function g(arg.func);
                calls.calls[g.name()] = g;
            }
        }
    }

    if (!recursive) {
        env.insert(calls.calls.begin(), calls.calls.end());
    } else {
        env[f.name()] = f;

        for (map<string, Function>::const_iterator iter = calls.calls.begin();
             iter != calls.calls.end(); ++iter) {
            populate_environment(iter->second, env);
        }
    }
}

map<string, Function> find_transitive_calls(Function f) {
    map<string, Function> res;
    populate_environment(f, res, true);
    return res;
}

map<string, Function> find_direct_calls(Function f) {
    map<string, Function> res;
    populate_environment(f, res, false);
    return res;
}

}
}
