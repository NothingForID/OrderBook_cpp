#include <cstdint>
#include <deque>
#include <map>
#include <vector>

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
	using Level = std::deque<Order>; // 同价订单队列，按提交先后排序  
	std::map<Price, Level> asks_; // 卖盘, key 升序, begin 指向最低价  
	std::map<Price, Level, std::greater<>> bids_; // 买盘, key 降序, begin 指向最高价  

	public:
		// 提交一个订单，返回这次提交产生的所有成交
		std::vector<Trade> submit(Order o) { // 不被多线程调用
			std::vector<Trade> trades; // 存放成交订单
			if (o.side == Side::Buy) { // 处理买单
				while ( // 步骤1 判断价格和盘口余量
							o.qty > 0 // 买单手数剩余大于0
							&& !asks_.empty() // 卖盘非空
							&& o.price >= asks_.begin()->first // 卖一价格低于买单报价
				) {
					auto& [px, level] = *asks_.begin(); // 结构化绑定
					Order& rest = level.front(); // 引用选取队首卖单
					// 步骤2 手数取小作减
					Qty fill = std::min(o.qty, rest.qty); // 手数取小
					trades.push_back({rest.id, o.id, px, fill}); // 记录成交
					o.qty -= fill; // 对提交订单的手数作减
					rest.qty -= fill; // 对队内订单的手数作减
					// 步骤3 移除队首手数余零的订单
					if (rest.qty == 0) {
						level.pop_front();
						if (level.empty()) { // 若卖一空
							asks_.erase(asks_.begin()); // 则删除原有的卖一档
						}
					}
				}
				// 步骤4 将有余的提交订单挂进买盘
				if (o.qty > 0) {
					// 新建价格档位订单队列
					Level& level = o.side == Side::Buy ? bids_[o.price]
																							:asks_[o.price];
					level.push_back(o); // 将提交的余单
				}
				return trades;
			}
		};
};


