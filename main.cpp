#include "numa.hpp"

class Task
{
	public:
		Task(NumaNode& node) : node_(node),timer_(node.io_service()) {};

		void	run()
		{
			timer_.expires_from_now(boost::posix_time::seconds(1));
			timer_.async_wait( [this] (const boost::system::error_code& ) {
				std::cout << "task is running on cpu=" << sched_getcpu() << std::endl;
				run();
				});
		}

	private:
		NumaNode						&node_;
		boost::asio::deadline_timer		timer_;
};

int main(int argc, char** argv)
{
	uint32_t		nodeId = 0;

	if (argc > 1)
	{
		nodeId = atoi(argv[1]);
	}

	std::cout << "Chosen NUMA node=" << nodeId << std::endl;

	// create NUMA node
	NumaNode	node(nodeId);
	node.set_num_threads(4);
	node.except_cpu("0,8,16,24");
	node.run();

	// create task on NUMA
	Task	task(node);
	task.run();

	node.join_all();

	return 0;
}
