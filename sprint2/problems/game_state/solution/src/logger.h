#pragma once

#include <iostream>
#include <string_view>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace json = boost::json;
using namespace std::literals;

// Атрибут для дополнительных данных в формате JSON
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
// Атрибут для временной метки
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

// Кастомный форматер для вывода в JSON
inline void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_record;

    // Добавляем временную метку
    auto ts = rec[timestamp];
    if (ts) {
        log_record["timestamp"] = to_iso_extended_string(*ts);
    }

    // Добавляем дополнительные данные
    auto data = rec[additional_data];
    if (data && !data->is_null()) {
        log_record["data"] = *data;
    } else {
        log_record["data"] = json::object{};
    }

    // Добавляем само сообщение
    log_record["message"] = *rec[logging::expressions::smessage];

    strm << json::serialize(log_record);
}

// Функция инициализации логгера
inline void InitBoostLog() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        keywords::format = &MyFormatter,
        keywords::auto_flush = true
    );
}