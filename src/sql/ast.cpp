#include "ast.h"

namespace blueis {
namespace sql {

Expr Expr::ident(const std::string& name) {
    Expr e;
    e.kind = Kind::Ident;
    e.ident_name = name;
    return e;
}

Expr Expr::literal(const std::string& v) {
    Expr e;
    e.kind = Kind::Literal;
    e.literal_val = v;
    return e;
}

Expr Expr::literal(int64_t v) {
    Expr e;
    e.kind = Kind::Literal;
    e.literal_val = v;
    return e;
}

Expr Expr::binary(const std::string& op, Expr l, Expr r) {
    Expr e;
    e.kind = Kind::Binary;
    e.op = op;
    e.left = std::make_unique<Expr>(std::move(l));
    e.right = std::make_unique<Expr>(std::move(r));
    return e;
}

} // namespace sql
} // namespace blueis
