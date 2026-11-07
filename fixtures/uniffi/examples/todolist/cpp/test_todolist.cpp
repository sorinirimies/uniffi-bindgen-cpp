// Port of uniffi-rs v0.31.2 examples/todolist/tests/bindings/test_todolist.py
#include "check.hpp"
#include "todolist.hpp"

using namespace todolist;

int main() {
    auto todo = TodoList::new_();
    TodoEntry entry{"Write bindings for strings in records"};

    todo->add_item("Write python bindings");
    CHECK_EQ(todo->get_last(), "Write python bindings");
    todo->add_item("Write tests for bindings");
    CHECK_EQ(todo->get_last(), "Write tests for bindings");
    todo->add_entry(entry);
    CHECK_EQ(todo->get_last(), "Write bindings for strings in records");
    CHECK_EQ(todo->get_last_entry().text, "Write bindings for strings in records");

    const std::string unicode =
        "Test \xC3\x9Cnicode h\xC3\xA0ndling without an entry can't believe I didn't test this at first \xF0\x9F\xA4\xA3";
    todo->add_item(unicode);
    CHECK_EQ(todo->get_last(), unicode);

    const std::string unicode_entry =
        "Test \xC3\x9Cnicode h\xC3\xA0ndling in an entry can't believe I didn't test this at first \xF0\x9F\xA4\xA3";
    todo->add_entry(TodoEntry{unicode_entry});
    CHECK_EQ(todo->get_last_entry().text, unicode_entry);

    auto todo2 = TodoList::new_();
    CHECK(todo != todo2);

    CHECK(!get_default_list().has_value());
    set_default_list(todo);
    CHECK(todo->get_items() == (*get_default_list())->get_items());

    todo2->make_default();
    CHECK(todo2->get_items() == (*get_default_list())->get_items());

    todo->add_item("Test liveness after being demoted from default");
    CHECK_EQ(todo->get_last(), "Test liveness after being demoted from default");

    todo2->add_item("Test shared state through local vs default reference");
    CHECK_EQ((*get_default_list())->get_last(), "Test shared state through local vs default reference");

    // Errors: an empty list has no last item.
    CHECK_THROWS(TodoList::new_()->get_last(), TodoError_EmptyTodoList);
    return check::report();
}
