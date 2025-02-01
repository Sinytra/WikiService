/**
 *
 *  Project.h
 *  DO NOT EDIT. This file is generated by drogon_ctl
 *
 */

#pragma once
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/BaseBuilder.h>
#ifdef __cpp_impl_coroutine
#include <drogon/orm/CoroMapper.h>
#endif
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <iostream>

namespace drogon
{
namespace orm
{
class DbClient;
using DbClientPtr = std::shared_ptr<DbClient>;
}
}
namespace drogon_model
{
namespace postgres
{
class User;
class UserProject;

class Project
{
  public:
    struct Cols
    {
        static const std::string _id;
        static const std::string _name;
        static const std::string _source_path;
        static const std::string _source_repo;
        static const std::string _source_branch;
        static const std::string _is_community;
        static const std::string _type;
        static const std::string _platforms;
        static const std::string _search_vector;
        static const std::string _created_at;
        static const std::string _is_public;
        static const std::string _modid;
    };

    static const int primaryKeyNumber;
    static const std::string tableName;
    static const bool hasPrimaryKey;
    static const std::string primaryKeyName;
    using PrimaryKeyType = std::string;
    const PrimaryKeyType &getPrimaryKey() const;

    /**
     * @brief constructor
     * @param r One row of records in the SQL query result.
     * @param indexOffset Set the offset to -1 to access all columns by column names,
     * otherwise access all columns by offsets.
     * @note If the SQL is not a style of 'select * from table_name ...' (select all
     * columns by an asterisk), please set the offset to -1.
     */
    explicit Project(const drogon::orm::Row &r, const ssize_t indexOffset = 0) noexcept;

    /**
     * @brief constructor
     * @param pJson The json object to construct a new instance.
     */
    explicit Project(const Json::Value &pJson) noexcept(false);

    /**
     * @brief constructor
     * @param pJson The json object to construct a new instance.
     * @param pMasqueradingVector The aliases of table columns.
     */
    Project(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false);

    Project() = default;

    void updateByJson(const Json::Value &pJson) noexcept(false);
    void updateByMasqueradedJson(const Json::Value &pJson,
                                 const std::vector<std::string> &pMasqueradingVector) noexcept(false);
    static bool validateJsonForCreation(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForCreation(const Json::Value &,
                                                const std::vector<std::string> &pMasqueradingVector,
                                                    std::string &err);
    static bool validateJsonForUpdate(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForUpdate(const Json::Value &,
                                          const std::vector<std::string> &pMasqueradingVector,
                                          std::string &err);
    static bool validJsonOfField(size_t index,
                          const std::string &fieldName,
                          const Json::Value &pJson,
                          std::string &err,
                          bool isForCreation);

    /**  For column id  */
    ///Get the value of the column id, returns the default value if the column is null
    const std::string &getValueOfId() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getId() const noexcept;
    ///Set the value of the column id
    void setId(const std::string &pId) noexcept;
    void setId(std::string &&pId) noexcept;

    /**  For column name  */
    ///Get the value of the column name, returns the default value if the column is null
    const std::string &getValueOfName() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getName() const noexcept;
    ///Set the value of the column name
    void setName(const std::string &pName) noexcept;
    void setName(std::string &&pName) noexcept;

    /**  For column source_path  */
    ///Get the value of the column source_path, returns the default value if the column is null
    const std::string &getValueOfSourcePath() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getSourcePath() const noexcept;
    ///Set the value of the column source_path
    void setSourcePath(const std::string &pSourcePath) noexcept;
    void setSourcePath(std::string &&pSourcePath) noexcept;

    /**  For column source_repo  */
    ///Get the value of the column source_repo, returns the default value if the column is null
    const std::string &getValueOfSourceRepo() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getSourceRepo() const noexcept;
    ///Set the value of the column source_repo
    void setSourceRepo(const std::string &pSourceRepo) noexcept;
    void setSourceRepo(std::string &&pSourceRepo) noexcept;

    /**  For column source_branch  */
    ///Get the value of the column source_branch, returns the default value if the column is null
    const std::string &getValueOfSourceBranch() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getSourceBranch() const noexcept;
    ///Set the value of the column source_branch
    void setSourceBranch(const std::string &pSourceBranch) noexcept;
    void setSourceBranch(std::string &&pSourceBranch) noexcept;

    /**  For column is_community  */
    ///Get the value of the column is_community, returns the default value if the column is null
    const bool &getValueOfIsCommunity() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<bool> &getIsCommunity() const noexcept;
    ///Set the value of the column is_community
    void setIsCommunity(const bool &pIsCommunity) noexcept;

    /**  For column type  */
    ///Get the value of the column type, returns the default value if the column is null
    const std::string &getValueOfType() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getType() const noexcept;
    ///Set the value of the column type
    void setType(const std::string &pType) noexcept;
    void setType(std::string &&pType) noexcept;

    /**  For column platforms  */
    ///Get the value of the column platforms, returns the default value if the column is null
    const std::string &getValueOfPlatforms() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getPlatforms() const noexcept;
    ///Set the value of the column platforms
    void setPlatforms(const std::string &pPlatforms) noexcept;
    void setPlatforms(std::string &&pPlatforms) noexcept;

    /**  For column search_vector  */
    ///Get the value of the column search_vector, returns the default value if the column is null
    const std::string &getValueOfSearchVector() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getSearchVector() const noexcept;
    ///Set the value of the column search_vector
    void setSearchVector(const std::string &pSearchVector) noexcept;
    void setSearchVector(std::string &&pSearchVector) noexcept;
    void setSearchVectorToNull() noexcept;

    /**  For column created_at  */
    ///Get the value of the column created_at, returns the default value if the column is null
    const ::trantor::Date &getValueOfCreatedAt() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<::trantor::Date> &getCreatedAt() const noexcept;
    ///Set the value of the column created_at
    void setCreatedAt(const ::trantor::Date &pCreatedAt) noexcept;

    /**  For column is_public  */
    ///Get the value of the column is_public, returns the default value if the column is null
    const bool &getValueOfIsPublic() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<bool> &getIsPublic() const noexcept;
    ///Set the value of the column is_public
    void setIsPublic(const bool &pIsPublic) noexcept;

    /**  For column modid  */
    ///Get the value of the column modid, returns the default value if the column is null
    const std::string &getValueOfModid() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getModid() const noexcept;
    ///Set the value of the column modid
    void setModid(const std::string &pModid) noexcept;
    void setModid(std::string &&pModid) noexcept;


    static size_t getColumnNumber() noexcept {  return 12;  }
    static const std::string &getColumnName(size_t index) noexcept(false);

    Json::Value toJson() const;
    Json::Value toMasqueradedJson(const std::vector<std::string> &pMasqueradingVector) const;
    /// Relationship interfaces
    std::vector<std::pair<User,UserProject>> getUsers(const drogon::orm::DbClientPtr &clientPtr) const;
    void getUsers(const drogon::orm::DbClientPtr &clientPtr,
                  const std::function<void(std::vector<std::pair<User,UserProject>>)> &rcb,
                  const drogon::orm::ExceptionCallback &ecb) const;
  private:
    friend drogon::orm::Mapper<Project>;
    friend drogon::orm::BaseBuilder<Project, true, true>;
    friend drogon::orm::BaseBuilder<Project, true, false>;
    friend drogon::orm::BaseBuilder<Project, false, true>;
    friend drogon::orm::BaseBuilder<Project, false, false>;
#ifdef __cpp_impl_coroutine
    friend drogon::orm::CoroMapper<Project>;
#endif
    static const std::vector<std::string> &insertColumns() noexcept;
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const;
    const std::vector<std::string> updateColumns() const;
    void updateArgs(drogon::orm::internal::SqlBinder &binder) const;
    ///For mysql or sqlite3
    void updateId(const uint64_t id);
    std::shared_ptr<std::string> id_;
    std::shared_ptr<std::string> name_;
    std::shared_ptr<std::string> sourcePath_;
    std::shared_ptr<std::string> sourceRepo_;
    std::shared_ptr<std::string> sourceBranch_;
    std::shared_ptr<bool> isCommunity_;
    std::shared_ptr<std::string> type_;
    std::shared_ptr<std::string> platforms_;
    std::shared_ptr<std::string> searchVector_;
    std::shared_ptr<::trantor::Date> createdAt_;
    std::shared_ptr<bool> isPublic_;
    std::shared_ptr<std::string> modid_;
    struct MetaData
    {
        const std::string colName_;
        const std::string colType_;
        const std::string colDatabaseType_;
        const ssize_t colLength_;
        const bool isAutoVal_;
        const bool isPrimaryKey_;
        const bool notNull_;
    };
    static const std::vector<MetaData> metaData_;
    bool dirtyFlag_[12]={ false };
  public:
    static const std::string &sqlForFindingByPrimaryKey()
    {
        static const std::string sql="select * from " + tableName + " where id = $1";
        return sql;
    }

    static const std::string &sqlForDeletingByPrimaryKey()
    {
        static const std::string sql="delete from " + tableName + " where id = $1";
        return sql;
    }
    std::string sqlForInserting(bool &needSelection) const
    {
        std::string sql="insert into " + tableName + " (";
        size_t parametersCount = 0;
        needSelection = false;
        if(dirtyFlag_[0])
        {
            sql += "id,";
            ++parametersCount;
        }
        if(dirtyFlag_[1])
        {
            sql += "name,";
            ++parametersCount;
        }
        if(dirtyFlag_[2])
        {
            sql += "source_path,";
            ++parametersCount;
        }
        if(dirtyFlag_[3])
        {
            sql += "source_repo,";
            ++parametersCount;
        }
        if(dirtyFlag_[4])
        {
            sql += "source_branch,";
            ++parametersCount;
        }
        sql += "is_community,";
        ++parametersCount;
        if(!dirtyFlag_[5])
        {
            needSelection=true;
        }
        if(dirtyFlag_[6])
        {
            sql += "type,";
            ++parametersCount;
        }
        if(dirtyFlag_[7])
        {
            sql += "platforms,";
            ++parametersCount;
        }
        if(dirtyFlag_[8])
        {
            sql += "search_vector,";
            ++parametersCount;
        }
        sql += "created_at,";
        ++parametersCount;
        if(!dirtyFlag_[9])
        {
            needSelection=true;
        }
        sql += "is_public,";
        ++parametersCount;
        if(!dirtyFlag_[10])
        {
            needSelection=true;
        }
        sql += "modid,";
        ++parametersCount;
        if(!dirtyFlag_[11])
        {
            needSelection=true;
        }
        if(parametersCount > 0)
        {
            sql[sql.length()-1]=')';
            sql += " values (";
        }
        else
            sql += ") values (";

        int placeholder=1;
        char placeholderStr[64];
        size_t n=0;
        if(dirtyFlag_[0])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[1])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[2])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[3])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[4])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[5])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        else
        {
            sql +="default,";
        }
        if(dirtyFlag_[6])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[7])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[8])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[9])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        else
        {
            sql +="default,";
        }
        if(dirtyFlag_[10])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        else
        {
            sql +="default,";
        }
        if(dirtyFlag_[11])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        else
        {
            sql +="default,";
        }
        if(parametersCount > 0)
        {
            sql.resize(sql.length() - 1);
        }
        if(needSelection)
        {
            sql.append(") returning *");
        }
        else
        {
            sql.append(1, ')');
        }
        LOG_TRACE << sql;
        return sql;
    }
};
} // namespace postgres
} // namespace drogon_model
