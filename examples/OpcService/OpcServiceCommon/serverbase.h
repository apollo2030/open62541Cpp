#ifndef SERVERBASE_H
#define SERVERBASE_H
#include "designwt.h"
#include "dialoghelper.h"
#include "opcservicecommon.h"
#include <Wt/WBootstrapTheme>
namespace MRL {
template <typename T>
/**
 * The ServerBase class
 */
class ServerBase : public Wt::WApplication {
    std::unique_ptr<T> _frame;
    std::unique_ptr<Wt::WBootstrapTheme> _theme; // the theme
public:
    ServerBase(const Wt::WEnvironment &env) :  Wt::WApplication(env) {
        _theme = std::make_unique<Wt::WBootstrapTheme>(nullptr);
        _theme->setVersion(Wt::WBootstrapTheme::Version3);
        setTheme(_theme.get());
        _frame = std::make_unique<T>(root());
    }
    T *frame() {
        return _frame.get();
    }
};
}

#endif // SERVERBASE_H
