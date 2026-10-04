#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Cache.h"

class Router {
public:
    explicit Router(Cache& cache);

    HttpResponse route(const HttpRequest& request) const;

private:
    Cache& cache;
};