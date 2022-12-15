#pragma once

namespace spieler
{

	class Logger
	{
	public:
		SPIELER_NON_CONSTRUCTABLE(Logger);

	public:
		enum class Severity
		{
			Info,
			Warning,
			Critical
		};

	public:
		template <typename... Args>
		static void Log(Severity severity, std::string_view filepath, uint32_t line, std::string_view format, Args&&... args);

	private:
		static void LogImpl(Severity severity, std::string_view filepath, uint32_t line, std::string_view message);
	};

} // namespace spieler

#define SPIELER_INFO(format, ...) ::spieler::Logger::Log(::spieler::Logger::Severity::Info, __FILE__, __LINE__, format, __VA_ARGS__)
#define SPIELER_WARNING(format, ...) ::spieler::Logger::Log(::spieler::Logger::Severity::Warning, __FILE__, __LINE__, format, __VA_ARGS__)
#define SPIELER_CRITICAL(format, ...) ::spieler::Logger::Log(::spieler::Logger::Severity::Critical, __FILE__, __LINE__, format, __VA_ARGS__)

#include "logger.inl"
