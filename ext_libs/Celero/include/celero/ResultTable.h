#ifndef H_CELERO_RESULTTABLE_H
#define H_CELERO_RESULTTABLE_H

// www.helleboreconsulting.com

#include <celero/Pimpl.h>
#include <string>

namespace celero
{
	///
	/// \class ResultTable
	///
	/// \author	John Farrier
	///
	class ResultTable
	{
		public:
			///
			/// Singleton 
			///
			static ResultTable& Instance();

			///
			///
			///
			void setFileName(const std::string& x);

			///
			///
			///
			void add(const std::string& groupName, const std::string& runName, const uint32_t runSize, const double usPerCall);

			///
			///
			///
			void print();

		private:
			///
			/// Default Constructor
			///
			ResultTable();

			///
			/// Default Destructor
			///
			~ResultTable();

			///
			/// \brief	Pimpl Idiom
			///
			class Impl;

			///
			/// \brief	Pimpl Idiom
			///
			Pimpl<Impl> pimpl;	
	};
}

#endif
