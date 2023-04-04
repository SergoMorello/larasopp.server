#pragma once

// #include <boost/log/core.hpp>
// #include <boost/log/trivial.hpp>
// #include <boost/log/expressions.hpp>
// #include <boost/log/utility/setup/file.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace logging = boost::log;

class logs {
	public:
		logs() {
			logging::add_file_log
			(
				logging::keywords::file_name = "sample_%N.log",                                        /* file name pattern            */ 
				logging::keywords::rotation_size = 10 * 1024 * 1024,                                   /* rotate files every 10 MiB... */ 
				logging::keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0), /* ...or at midnight            */ 
				logging::keywords::auto_flush = true,
				logging::keywords::format = "[%TimeStamp%]: %Message%"                                 /* log record format            */ 
			);
			logging::core::get()->set_filter
			(
				logging::trivial::severity >= logging::trivial::info
			);
			logging::core::get();
		}
};