#include <sys/syscall.h>
#include "numa.hpp"

std::ostream& operator<<(std::ostream& os, const bitmask& bm)
{
    for(size_t i=0;i<10;++i)
    {
        os << numa_bitmask_isbitset(&bm, i);
    }
    return os;
}

void thread_initialize_fn(uint32_t nodeId, boost::asio::io_service *io_service)
{
	numa_run_on_node(nodeId);
	numa_set_preferred(nodeId);

	io_service->run();
}

void thread_io_run_fn(int numa_node_id, std::vector<uint32_t>& except, boost::asio::io_service* io_service)
{
	numa_run_on_node(numa_node_id);
	numa_set_preferred(numa_node_id);

	pid_t pid = syscall(SYS_gettid);

	struct bitmask *mask = numa_allocate_cpumask();
	numa_sched_getaffinity(pid, mask);

	for (uint32_t i : except)
	{
		numa_bitmask_clearbit(mask, i);
	}

	numa_sched_setaffinity(pid, mask);
	numa_free_cpumask(mask);

	io_service->run();
}

void
NumaNode::bind_node(uint32_t nodeId)
{
	pid_t			pid;
	cpu_set_t		set;
	struct bitmask *bitmask;

	CPU_ZERO(&set);

	bitmask = numa_allocate_cpumask();
	numa_node_to_cpus(nodeId, bitmask);

	for (int i=0; i<numa_num_configured_cpus();i++)
	{
		if (numa_bitmask_isbitset(bitmask, i))
		{
			CPU_SET(i, &set);
		}
	}

	pid = getpid();
	if ( sched_setaffinity(pid, sizeof(set), &set) ) {
		std::cerr << __FILE__ << ":" << __LINE__ << " sched_setaffinity() fail: " << strerror(errno) << std::endl;
	}

	numa_free_cpumask(bitmask);
}

void
NumaNode::bind_mem(uint32_t nodeId)
{
	unsigned long nodemask = 1L << nodeId;
	if ( set_mempolicy(MPOL_BIND, &nodemask, 8 * sizeof(nodemask)) ) {
		std::cerr << __FILE__ << ":" << __LINE__ << " set_mempolicy() fail: " << strerror(errno) << std::endl;
	}
}

void
NumaNode::create_io(boost::shared_ptr<boost::promise<bool>> p)
{
	io_ = new boost::asio::io_service;
	work_ = new boost::asio::io_service::work(*io_);
	strand_ = new boost::asio::io_service::strand(*io_);

	std::cout << "io's address is on node=" << find_node_from_address(io_) << std::endl;
	std::cout << "work's address is on node=" << find_node_from_address(work_) << std::endl;
	std::cout << "strand's address is on node=" << find_node_from_address(strand_) << std::endl;

	p->set_value(true);
}

NumaNode::NumaNode(uint32_t node_id)
{
	id_ = node_id;
	numThreads_ = 1;
}

NumaNode::~NumaNode()
{
	if (io_)
	{
		io_->stop();
	}
	threadGroup_.join_all();
}

void
NumaNode::initialize()
{
	boost::asio::io_service			io;
	boost::asio::io_service::work	work(io);
	boost::asio::io_service::strand	strand(io);

	numa_run_on_node(id_);

	do {
		if (numa_node_of_cpu(sched_getcpu()) == id_) break;
		pthread_yield();
	} while (1);

	int oldId = numa_preferred();
	numa_set_preferred(id_);

	boost::thread t(boost::bind(thread_initialize_fn, id_, &io));

	boost::shared_ptr<boost::promise<bool> >	p(new boost::promise<bool>());
	boost::unique_future<bool>					f = p->get_future();

	strand.post(boost::bind(&NumaNode::create_io, this, p));

	f.wait();

	io.stop();

	t.join();

	numa_set_preferred(oldId);
}

void
NumaNode::set_num_threads(uint32_t num)
{
	numThreads_ = num;
}

void
NumaNode::except_cpu(std::string cpuIdList)
{
	struct bitmask *mask = numa_parse_cpustring(cpuIdList.c_str());

	for (int i=0;i<numa_num_configured_cpus();i++)
	{
		if (numa_bitmask_isbitset(mask, i))
		{
			exceptCpus_.push_back(i);
		}
	}
}

void
NumaNode::run()
{
	initialize();

	int local_node_id = numa_preferred();
	numa_set_preferred(id_);

	for (int i=0; i<numThreads_; ++i)
	{
		threadGroup_.create_thread(boost::bind(thread_io_run_fn, id_, exceptCpus_, io_));
	}
	numa_set_preferred(local_node_id);
}

void
NumaNode::join_all()
{
	threadGroup_.join_all();
}

uint32_t
NumaNode::id()
{
	return id_;
}

boost::asio::io_service&
NumaNode::io_service()
{
	return *io_;
}

boost::asio::io_service::strand&
NumaNode::strand()
{
	return *strand_;
}

uint32_t
NumaNode::num_nodes()
{
	return numa_max_node() + 1;
}

uint32_t
NumaNode::find_node_from_address(void* ptr)
{
	uint32_t	node_id = -1;

#if 1
	get_mempolicy((int*)&node_id, NULL, 0, (void*)ptr, MPOL_F_NODE | MPOL_F_ADDR);
#else
	int		status[1];
	int		ret_code;

	status[0] = -1;
	ret_code = move_pages(0 /*self memory */, 1, &ptr, NULL, status, 0);
	node_id = status[0];
#endif

	return node_id;
}


