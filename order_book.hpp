#include <cstdint>
// #include <deque>
#include <list>
#include <map>
#include <vector>
#include <unordered_map>

using Id    = std::int64_t;
using Price = std::int64_t; // 单位是分
using Qty   = std::int64_t;

enum class Side {Buy, Sell };
enum class Type {
	GTC, // good to cancel, 撤单前有效
	IOC  // immediate or cancel, 立即成交并取消剩余
};

struct Order {	// 委托
	Id id;				// 单号，肯定不会重复
	Side side;		// 方向
	Type type;		// 类型
	Price price;	// 价格，肯定大于零
	Qty qty;			// 手数，肯定大于零
};

struct Trade {	// 成交
	Id resting;		// 原本在排队的单号
	Id incoming;	// 新来的单号
	Price price;	// 价格
	Qty qty;			// 手数
};

class OrderBook { // 订单簿
	// 订单规模 1e+5
	// 价格规模 1e+2
	using Level = std::list<Order>; // 同价订单队列，按提交先后排序  
	std::map<Price, Level> asks_; // 卖盘, key 升序, begin 指向最低价  
	std::map<Price, Level, std::greater<>> bids_; // 买盘, key 降序, begin 指向最高价  
	std::unordered_map<Id, Level::iterator> index_; // 订单索引表

	static bool crosses(const Order& o, Price px) {
	// 根据订单方向与对手盘一档比价，判断能否撮合
		return o.side == Side::Buy ? o.price >= px : o.price <= px;
	}

	template <class Book>
	void match(
		Order& o, // 新提交的订单
		Book& book, // 对手盘
		std::vector<Trade>& trades // 记录成交的容器
	) {
		while ( // 步骤1 判断价格和盘口余量
					o.qty > 0 // 提交订单的余量大于零
					&& !book.empty() // 对手盘非空
					&& crosses(o, book.begin()->first) // 比价满足撮合
				) {
					auto& [px, level] = *book.begin(); // 结构化绑定
					Order& rest = book.front(); // 引用选取队首卖单
					// 步骤2 手数取小作减
					Qty fill = std::min(o.qty, rest.qty); // 手数取小
					trades.push_back({rest.id, o.id, px, fill}); // 记录成交
					o.qty -= fill; // 对提交订单的手数作减
					rest.qty -= fill; // 对队内订单的手数作减
					// 步骤3 移除队首手数余零的订单
					if (rest.qty == 0) {
						index_.erase(rest.id); // 从索引表内删除完全成交的订单
						book.pop_front();
						if (book.empty()) { // 若对手盘一档为空
							book.erase(book.begin()); // 则删除原有的一档
						}
					}
				}
	}

	template <class Book>
	void remove(Book& book, Level::iterator it) {
		// 完成撤单函数 `cancel()` 的步骤2, 3, 5
		// 步骤02 用订单价格查找档位
		auto lv = book.find(it->price);
		// 步骤03 在档内删除订单
		lv->second.erase(it);
		// 步骤05 若档内无余单，则删档
		if (lv->second.empty()) book.erase(lv);
	}

	public:
		// 提交一个订单，返回这次提交产生的所有成交
		std::vector<Trade> submit(Order o) { // 不被多线程调用
			std::vector<Trade> trades; // 存放成交订单
			// 买单对手卖盘，卖单对手买盘
			if (o.side == Side::Buy) match(o, asks_, trades);
			else match(o, bids_, trades);
			// 步骤4 将有余的 GTC 提交订单挂进买盘
			if (o.qty > 0 && o.type == Type::GTC) {
				// 新建价格档位订单队列
				Level& level = o.side == Side::Buy ? bids_[o.price]
																						:asks_[o.price];
				index_[o.id] = level.insert(level.end(), o); // 将提交的余单挂入新档
			}
			return trades;
		}

		bool cancel(Id id) {
			// 步骤01 查索引表
			auto hit = index_.find(id);
			// 若查找不到，则返回否
			if (hit == index_.end()) return false;
			// 否则取迭代器，指向 Order 节点
			Level::iterator it = hit->second;
			// 根据交易方向选择盘口撤单 步骤02, 03, 05
			if (it->side == Side::Buy) remove(bids_, it);
			else remove(asks_, it);
			// 步骤04 在索引表内删除 id 
			index_.erase(hit);
			return true;
		}
};