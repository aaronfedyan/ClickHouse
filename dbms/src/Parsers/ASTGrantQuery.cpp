#include <Parsers/ASTGrantQuery.h>
#include <Parsers/ASTRoleList.h>
#include <Common/quoteString.h>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <map>


namespace DB
{
namespace
{
    using KeywordToColumnsMap = std::map<std::string_view /* keyword */, std::vector<std::string_view> /* columns */>;
    using TableToAccessMap = std::map<String /* database_and_table_name */, KeywordToColumnsMap>;

    TableToAccessMap prepareTableToAccessMap(const AccessRightsElements & elements)
    {
        TableToAccessMap res;
        for (const auto & element : elements)
        {
            String database_and_table_name;
            if (element.any_database)
            {
                if (element.any_table)
                    database_and_table_name = "*.*";
                else
                    database_and_table_name = "*." + backQuoteIfNeed(element.table);
            }
            else if (element.database.empty())
            {
                if (element.any_table)
                    database_and_table_name = "*";
                else
                    database_and_table_name = backQuoteIfNeed(element.table);
            }
            else
            {
                if (element.any_table)
                    database_and_table_name = backQuoteIfNeed(element.database) + ".*";
                else
                    database_and_table_name = backQuoteIfNeed(element.database) + "." + backQuoteIfNeed(element.table);
            }

            KeywordToColumnsMap & keyword_to_columns = res[database_and_table_name];
            for (const auto & keyword : element.access_flags.toKeywords())
                boost::range::push_back(keyword_to_columns[keyword], element.columns);
        }

        for (auto & keyword_to_columns : res | boost::adaptors::map_values)
        {
            for (auto & columns : keyword_to_columns | boost::adaptors::map_values)
                boost::range::sort(columns);
        }
        return res;
    }


    void formatColumnNames(const std::vector<std::string_view> & columns, const IAST::FormatSettings & settings)
    {
        if (columns.empty())
            return;

        settings.ostr << "(";
        bool need_comma_after_column_name = false;
        for (const auto & column : columns)
        {
            if (std::exchange(need_comma_after_column_name, true))
                settings.ostr << ", ";
            settings.ostr << backQuoteIfNeed(column);
        }
        settings.ostr << ")";
    }
}


String ASTGrantQuery::getID(char) const
{
    return "GrantQuery";
}


ASTPtr ASTGrantQuery::clone() const
{
    return std::make_shared<ASTGrantQuery>(*this);
}


void ASTGrantQuery::formatImpl(const FormatSettings & settings, FormatState &, FormatStateStacked) const
{
    settings.ostr << (settings.hilite ? hilite_keyword : "") << ((kind == Kind::GRANT) ? "GRANT" : "REVOKE")
                  << (settings.hilite ? hilite_none : "") << " ";

    if (grant_option && (kind == Kind::REVOKE))
        settings.ostr << (settings.hilite ? hilite_keyword : "") << "GRANT OPTION FOR " << (settings.hilite ? hilite_none : "");

    bool need_comma = false;
    for (const auto & [database_and_table, keyword_to_columns] : prepareTableToAccessMap(access_rights_elements))
    {
        for (const auto & [keyword, columns] : keyword_to_columns)
        {
            if (std::exchange(need_comma, true))
                settings.ostr << ", ";

            settings.ostr << (settings.hilite ? hilite_keyword : "") << keyword << (settings.hilite ? hilite_none : "");
            formatColumnNames(columns, settings);
        }

        settings.ostr << (settings.hilite ? hilite_keyword : "") << " ON " << (settings.hilite ? hilite_none : "") << database_and_table;
    }

    settings.ostr << (settings.hilite ? hilite_keyword : "") << ((kind == Kind::GRANT) ? " TO " : " FROM ") << (settings.hilite ? hilite_none : "");
    to_roles->format(settings);

    if (grant_option && (kind == Kind::GRANT))
        settings.ostr << (settings.hilite ? hilite_keyword : "") << " WITH GRANT OPTION" << (settings.hilite ? hilite_none : "");
}
}
