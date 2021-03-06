/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_EMPTY_TUPLE_HPP
#define CPPA_EMPTY_TUPLE_HPP

#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/singleton_mixin.hpp"

namespace cppa { namespace detail {

class empty_tuple : public singleton_mixin<empty_tuple, abstract_tuple> {

    friend class singleton_manager;
    friend class singleton_mixin<empty_tuple, abstract_tuple>;

    typedef singleton_mixin<empty_tuple, abstract_tuple> super;

 public:

    using abstract_tuple::const_iterator;

    size_t size() const;
    void* mutable_at(size_t);
    abstract_tuple* copy() const;
    const void* at(size_t) const;
    bool equals(const abstract_tuple& other) const;
    const uniform_type_info* type_at(size_t) const;
    const std::type_info* type_token() const;
    const std::string* tuple_type_names() const;

 private:

    empty_tuple();

    inline void initialize() { ref(); }
    inline void destroy() { deref(); }

};

} } // namespace cppa::detail

#endif // CPPA_EMPTY_TUPLE_HPP
