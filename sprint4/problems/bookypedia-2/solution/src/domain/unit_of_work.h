#pragma once

#include <memory>

#include "author.h"
#include "book.h"

namespace domain {

class UnitOfWork {
public:
    virtual AuthorRepository& Authors() = 0;
    virtual BookRepository& Books() = 0;
    virtual void Commit() = 0;

    virtual ~UnitOfWork() = default;
};

class UnitOfWorkFactory {
public:
    virtual std::unique_ptr<UnitOfWork> CreateUnitOfWork() const = 0;

    virtual ~UnitOfWorkFactory() = default;
};

}  // namespace domain
