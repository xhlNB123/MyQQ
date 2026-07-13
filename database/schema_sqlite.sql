-- =====================================================================
-- MyQQ 数据库 —— SQLite 版建表脚本（服务端启动时自动执行）
-- 对应 schema.sql 的 SQL Server 结构，语法适配 SQLite。
-- =====================================================================

CREATE TABLE IF NOT EXISTS Star (
    StarId   INTEGER PRIMARY KEY,
    StarName TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS BloodType (
    BloodTypeId   INTEGER PRIMARY KEY,
    BloodTypeName TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS FriendshipPolicy (
    PolicyId   INTEGER PRIMARY KEY,
    PolicyName TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS MessageType (
    TypeId   INTEGER PRIMARY KEY,
    TypeName TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Users (
    UserId      INTEGER PRIMARY KEY AUTOINCREMENT,   -- QQ 号
    Account     TEXT NOT NULL UNIQUE,
    Password    TEXT NOT NULL,
    NickName    TEXT NOT NULL,
    Gender      INTEGER DEFAULT 0,                   -- 0 未知 1 男 2 女
    StarId      INTEGER,
    BloodTypeId INTEGER,
    PolicyId    INTEGER DEFAULT 1,
    AvatarPath  TEXT,
    Signature   TEXT,
    Status      INTEGER DEFAULT 0,                   -- 0 离线 1 在线 2 忙碌
    CreateTime  TEXT DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS Friends (
    Id       INTEGER PRIMARY KEY AUTOINCREMENT,
    UserId   INTEGER NOT NULL,
    FriendId INTEGER NOT NULL,
    Remark   TEXT,
    GroupName TEXT DEFAULT '我的好友',
    AddTime  TEXT DEFAULT (datetime('now','localtime')),
    UNIQUE(UserId, FriendId)
);

CREATE TABLE IF NOT EXISTS Messages (
    MsgId      INTEGER PRIMARY KEY AUTOINCREMENT,
    SenderId   INTEGER NOT NULL,
    ReceiverId INTEGER NOT NULL,
    TypeId     INTEGER NOT NULL,
    Content    TEXT NOT NULL,
    SendTime   TEXT DEFAULT (datetime('now','localtime')),
    IsRead     INTEGER DEFAULT 0
);

-- QQ 号从 10001 起（模拟真实 QQ 号）
INSERT INTO sqlite_sequence(name, seq)
    SELECT 'Users', 10000
    WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name='Users');

-- 字典数据
INSERT OR IGNORE INTO Star (StarId, StarName) VALUES
 (1,'白羊座'),(2,'金牛座'),(3,'双子座'),(4,'巨蟹座'),
 (5,'狮子座'),(6,'处女座'),(7,'天秤座'),(8,'天蝎座'),
 (9,'射手座'),(10,'摩羯座'),(11,'水瓶座'),(12,'双鱼座');

INSERT OR IGNORE INTO BloodType (BloodTypeId, BloodTypeName) VALUES
 (1,'A'),(2,'B'),(3,'O'),(4,'AB'),(5,'未知');

INSERT OR IGNORE INTO FriendshipPolicy (PolicyId, PolicyName) VALUES
 (1,'允许任何人添加'),(2,'需要验证信息'),(3,'拒绝任何人添加');

INSERT OR IGNORE INTO MessageType (TypeId, TypeName) VALUES
 (1,'文本消息'),(2,'系统消息'),(3,'好友请求'),(4,'好友请求回执');
