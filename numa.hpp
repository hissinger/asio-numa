#ifndef _NUMA_HPP_
#define _NUMA_HPP_

#include <numa.h>
#include <numaif.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

class NumaNode
{
	public:
		NumaNode(uint32_t node_id);
		~NumaNode();

		void								set_num_threads(uint32_t num);
		void								except_cpu(std::string cpuIdList);
		void								run();
		void								join_all();
		uint32_t							id();
		boost::asio::io_service&			io_service();
		boost::asio::io_service::strand&	strand();
		static uint32_t						num_nodes();
		static uint32_t						find_node_from_address(void* ptr);

		template<typename F, typename A>
		void run(F func, A arg)
		{
			numa_run_on_node(id_);

			int oldId = numa_preferred();
			numa_set_preferred(id_);

			boost::shared_ptr<boost::promise<bool> >	p(new boost::promise<bool>());
			boost::unique_future<bool>					f = p->get_future();

			strand_->post( [this, p, func, arg]() {
				boost::bind(func, arg)();
				p->set_value(true);
				});
					
			f.wait();

			numa_set_preferred(oldId);
		}

	private:
		uint32_t							id_;
		boost::asio::io_service 			*io_;
		boost::asio::io_service::work		*work_;
		boost::asio::io_service::strand		*strand_;
		boost::thread_group					threadGroup_;
		std::vector<uint32_t>				exceptCpus_;
		uint32_t							numThreads_;

	private:
		void	initialize();
		void	create_io(boost::shared_ptr<boost::promise<bool>> p);
		void	bind_node(uint32_t nodeId);
		void	bind_mem(uint32_t nodeId);
};

class NumaScopedPreferred
{
	public:
		NumaScopedPreferred(int node_id) 
		{
			oldId_ = numa_preferred();
			numa_set_preferred(node_id);
		}

		~NumaScopedPreferred()
		{
			numa_set_preferred(oldId_);
		}

	private:
		int	oldId_;
};

std::ostream& operator<<(std::ostream& os, const bitmask& bm);


#endif // _NUMA_HPP_
