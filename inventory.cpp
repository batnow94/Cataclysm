#include "inventory.h"
#include "game.h"
#include "keypress.h"

using namespace cataclysm;

DEFINE_ACID_ASSIGN_W_MOVE(inventory);

item& inventory::operator[] (int i)
{
    assert(0 <= i && items.size() > i);
    assert(!items[i].empty());
    return items[i].front();
}

const item& inventory::operator[] (int i) const
{
    assert(0 <= i && items.size() > i);
    assert(!items[i].empty());
	return items[i].front();
}

std::vector<item>& inventory::stack_at(int i)
{
 assert(0 <= i && items.size() > i);
 return items[i];
}

std::vector<item> inventory::const_stack(int i) const
{
 assert(0 <= i && items.size() > i);
 return items[i];
}

#if DEAD_FUNC
std::vector<item> inventory::as_vector()
{
 std::vector<item> ret;
 for (const auto& stack : items) {
  for (const auto& it : stack) ret.push_back(it);
 }
 return ret;
}
#endif

int inventory::num_items() const
{
 int ret = 0;
 for (const auto& stack : items) ret += stack.size();

 return ret;
}

inventory& inventory::operator+= (const inventory &rhs)
{
 for (size_t i = 0; i < rhs.size(); i++) add_stack(rhs.const_stack(i));
 return *this;
}

inventory& inventory::operator+= (const std::vector<item> &rhs)
{
 for (const auto& it : rhs) add_item(it);
 return *this;
}

inventory& inventory::operator+= (const item &rhs)
{
 add_item(rhs);
 return *this;
}

// 2018-08-16: leave these out-of-line for now (until we have a speed problem prioritize executable size)
inventory inventory::operator+ (const inventory &rhs) const
{
 return inventory(*this) += rhs;
}

inventory inventory::operator+ (const std::vector<item> &rhs) const
{
 return inventory(*this) += rhs;
}

inventory inventory::operator+ (const item &rhs) const
{
 return inventory(*this) += rhs;
}

void inventory::add_stack(const std::vector<item>& newits)
{
 for (const auto& it : newits) add_item(it, true);
}

void inventory::_add_item(item&& newit, bool keep_invlet)
{
    if (newit.is_style()) throw std::logic_error("tried to add style to inventory");
    if (keep_invlet && !newit.invlet_is_okay()) assign_empty_invlet(newit); // Keep invlet is true, but invlet is invalid!

    for (auto& stack : items) {
        item& first = stack[0];
        if (first.stacks_with(newit)) {
            newit.invlet = first.invlet;
            stack.push_back(std::move(newit));
            return;
        }
        else if (keep_invlet && first.invlet == newit.invlet)
            assign_empty_invlet(first);
    }
    if (!newit.invlet_is_okay() || index_by_letter(newit.invlet) != -1)
        assign_empty_invlet(newit);

    // C++20: no relevant move-constructor for std::vector
    std::vector<item> newstack;
    newstack.push_back(std::move(newit));
    items.push_back(std::move(newstack));
}

void inventory::restack(player *p)
{
 inventory tmp;
 for (size_t i = 0; i < size(); i++) {
  for (const auto& it : items[i]) tmp.add_item(it);
 }
 clear();
 if (p) {
// Doing it backwards will preserve older items' invlet
/*
  for (size_t i = tmp.size() - 1; i >= 0; i--) {
   if (p->has_weapon_or_armor(tmp[i].invlet)) 
    tmp.assign_empty_invlet(tmp[i], p);
  }
*/
  for (size_t i = 0; i < tmp.size(); i++) {
   if (!tmp[i].invlet_is_okay() || p->has_weapon_or_armor(tmp[i].invlet)) {
    tmp.assign_empty_invlet(tmp[i], p);
	for (auto& it : tmp.items[i]) it.invlet = tmp[i].invlet;
   }
  }
 }
 for (size_t i = 0; i < tmp.size(); i++)
  items.push_back(tmp.items[i]);
}

inventory::inventory(const GPS_loc& origin, int range) {
    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            GPS_loc src = origin + point(x, y);
            for (auto& obj : src.items_at()) if (!obj.made_of(LIQUID)) add_item(obj);
            // Kludge for now!
            if (fd_fire == src.field_at().type) {
                item fire(item::types[itm_fire], 0);
                fire.charges = 1;
                add_item(std::move(fire));
            }
        }
    }
}

void inventory::destroy_stack(int index)
{
    assert(0 <= index && items.size() > index);
    EraseAt(items, index);
}

item inventory::remove_item(int index)
{
 assert(0 <= index && items.size() > index);

 item ret(std::move(items[index][0]));
 EraseAt(items[index], 0);
 if (items[index].empty()) EraseAt(items, index);

 return ret;
}

item inventory::remove_item(int stack, int index)
{
 assert(0 <= stack && items.size() > stack);
 assert(0 <= index && items[stack].size() > index);

 item ret = std::move(items[stack][index]);
 EraseAt(items[stack], index);
 if (items[stack].empty()) EraseAt(items, stack);

 return ret;
}

item inventory::remove_item_by_letter(char ch)
{
 for (int i = 0; i < items.size(); i++) {
  if (items[i][0].invlet == ch) {
   if (items[i].size() > 1)
    items[i][1].invlet = ch;
   return remove_item(i);
  }
 }

 return item::null;
}

int inventory::index_by_letter(char ch) const
{
 if (ch == KEY_ESCAPE) return -1;
 for (int i = 0; i < items.size(); i++) {
  if (items[i][0].invlet == ch) return i;
 }
 return -1;
}

int inventory::index_of(const item& it) const
{
    int i = -1;
    for (decltype(auto) stack : items) {
        ++i;
        if (&it == &(stack.front())) return i;
    }

    return -1;
}


int inventory::amount_of(itype_id it) const
{
 int count = 0;
 for (const auto& stack : items) {
  for (const auto& obj : stack) {
   if (obj.type->id == it) count++;
   for (const auto& within : obj.contents) {
    if (within.type->id == it) count++;
   }
  }
 }
 return count;
}

int inventory::charges_of(itype_id it) const
{
 int count = 0;
 for (const auto& stack : items) {
  for (const auto& obj : stack) {
   if (obj.type->id == it) count += (obj.charges < 0) ? 1 : obj.charges;
   for (const auto& within : obj.contents) {
    if (within.type->id == it) count += (within.charges < 0) ? 1 : within.charges;
   }
  }
 }
 return count;
}

void inventory::use_amount(itype_id it, int quantity, bool use_container)
{
 for (int i = 0; i < items.size() && quantity > 0; i++) {
  for (int j = 0; j < items[i].size() && quantity > 0; j++) {
// First, check contents
   bool used_item_contents = false;
   for (int k = 0; k < items[i][j].contents.size() && quantity > 0; k++) {
    if (items[i][j].contents[k].type->id == it) {
     quantity--;
     EraseAt(items[i][j].contents, k);
     k--;
     used_item_contents = true;
    }
   }
// Now check the item itself
   if (use_container && used_item_contents) {
    EraseAt(items[i], j);
    j--;
    if (items[i].empty()) {
     EraseAt(items, i);
     i--;
     j = 0;
    }
   } else if (items[i][j].type->id == it && quantity > 0) {
    quantity--;
    EraseAt(items[i], j);
    j--;
    if (items[i].empty()) {
     EraseAt(items, i);
     i--;
     j = 0;
    }
   }
  }
 }
}

unsigned int inventory::use_charges(itype_id it, int quantity)
{
    const int start_qty = quantity;

    ptrdiff_t i = items.size();
    while (0 < --i) {
        auto& outside = items[i];
        ptrdiff_t j = outside.size();
        while (0 < --j) {
            auto& obj = outside[j];
            if (auto code = obj.use_charges(it, quantity)) {
                if (0 > code) {
                    EraseAt(outside, j);
                    if (outside.empty()) { // should imply j = 0
                        EraseAt(items, i);
                        if (items.size() <= i) return start_qty - quantity;
                    }
                    if (0 < quantity) continue;
                }
                return start_qty;
            }
        }
    }
    return start_qty - quantity;
}
 
bool inventory::has_item(item *it) const
{
 for (int i = 0; i < items.size(); i++) {
  for (int j = 0; j < items[i].size(); j++) {
   if (it == &(items[i][j])) return true;
   for (int k = 0; k < items[i][j].contents.size(); k++) {
    if (it == &(items[i][j].contents[k])) return true;
   }
  }
 }
 return false;
}

void inventory::assign_empty_invlet(item &it, player *p)
{
 for (int ch = 'a'; ch <= 'z'; ch++) {
  if (index_by_letter(ch) == -1 && (!p || !p->has_weapon_or_armor(ch))) {
   it.invlet = ch;
   return;
  }
 }
 for (int ch = 'A'; ch <= 'Z'; ch++) {
  if (index_by_letter(ch) == -1 && (!p || !p->has_weapon_or_armor(ch))) {
   it.invlet = ch;
   return;
  }
 }
 it.invlet = '`';
}
