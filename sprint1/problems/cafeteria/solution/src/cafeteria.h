#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class HotDogOrder : public std::enable_shared_from_this<HotDogOrder> {
public:
    HotDogOrder(net::io_context& io, std::shared_ptr<GasCooker> cooker, Store& store, HotDogHandler handler)
        : io_{io}
        , strand_{net::make_strand(io)}
        , sausage_timer_{io_}
        , bread_timer_{io_}
        , gas_cooker_{std::move(cooker)}
        , store_{store}
        , handler_{std::move(handler)} {
    }

    void Execute() {
        sausage_ = store_.GetSausage();
        bread_ = store_.GetBread();

        BakeBread();
        CookSausage();
    }

private:
    void CookSausage() {
        sausage_->StartFry(*gas_cooker_, [self = shared_from_this()] {
            self->sausage_timer_.expires_after(Milliseconds{1500});
            self->sausage_timer_.async_wait(net::bind_executor(self->strand_, [self](sys::error_code ec) {
                self->OnSausageCooked(ec);
            }));
        });
    }

    void BakeBread() {
        bread_->StartBake(*gas_cooker_, [self = shared_from_this()] {
            self->bread_timer_.expires_after(Milliseconds{1000});
            self->bread_timer_.async_wait(net::bind_executor(self->strand_, [self](sys::error_code ec) {
                self->OnBreadBaked(ec);
            }));
        });
    }

    void OnSausageCooked(sys::error_code ec) {
        if (delivered_) return;
        if (!ec) {
            sausage_->StopFry();
            sausage_cooked_ = true;
        }
        CheckReadiness(ec);
    }

    void OnBreadBaked(sys::error_code ec) {
        if (delivered_) return;
        if (!ec) {
            bread_->StopBaking();
            bread_baked_ = true;
        }
        CheckReadiness(ec);
    }
    
    void CheckReadiness(sys::error_code ec) {
        if (delivered_) return;

        if (ec) {
            return Deliver(Result<HotDog>(std::make_exception_ptr(sys::system_error(ec))));
        }

        if (sausage_cooked_ && bread_baked_) {
            try {
                Deliver(Result<HotDog>({sausage_->GetId(), sausage_, bread_}));
            } catch (...) {
                Deliver(Result<HotDog>::FromCurrentException());
            }
        }
    }

    void Deliver(Result<HotDog> result) {
        delivered_ = true;
        net::post(io_, [handler = std::move(handler_), result = std::move(result)]() mutable {
            handler(std::move(result));
        });
    }

    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    net::steady_timer sausage_timer_;
    net::steady_timer bread_timer_;

    std::shared_ptr<GasCooker> gas_cooker_;
    Store& store_;
    HotDogHandler handler_;

    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;

    bool sausage_cooked_ = false;
    bool bread_baked_ = false;
    bool delivered_ = false;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io}
        , store_strand_{net::make_strand(io_)} { // Инициализируем strand для склада
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        // Отправляем задачу на получение ингредиентов в strand, чтобы избежать гонки данных
        net::post(store_strand_, [this, handler = std::move(handler)]() mutable {
            std::make_shared<HotDogOrder>(io_, gas_cooker_, store_, std::move(handler))->Execute();
        });
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    // Strand для синхронизации доступа к складу
    net::strand<net::io_context::executor_type> store_strand_;
};