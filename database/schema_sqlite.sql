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
    PolicyId    INTEGER DEFAULT 2,
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

-- 好友申请（好友关系只在接受后写入 Friends）
CREATE TABLE IF NOT EXISTS FriendRequests (
    RequestId          INTEGER PRIMARY KEY AUTOINCREMENT,
    SenderId           INTEGER NOT NULL,
    ReceiverId         INTEGER NOT NULL,
    VerifyText         TEXT NOT NULL DEFAULT '',
    Status             INTEGER NOT NULL DEFAULT 0, -- 0 pending / 1 accepted / 2 rejected
    CreatedTime        TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    HandledTime        TEXT,
    ResultAcknowledged INTEGER NOT NULL DEFAULT 0,
    CHECK (SenderId <> ReceiverId),
    FOREIGN KEY (SenderId) REFERENCES Users(UserId),
    FOREIGN KEY (ReceiverId) REFERENCES Users(UserId)
);

-- 文件/图片存储（Messages.FileId 关联）
CREATE TABLE IF NOT EXISTS Files (
    FileId     INTEGER PRIMARY KEY AUTOINCREMENT,
    OwnerId    INTEGER NOT NULL,
    FileName   TEXT NOT NULL,
    FileSize   INTEGER NOT NULL,
    Kind       INTEGER NOT NULL,          -- 1 图片 2 文件
    StorePath  TEXT NOT NULL,
    CreatedTime TEXT DEFAULT (datetime('now','localtime'))
);

-- ---------- 群聊 ----------
CREATE TABLE IF NOT EXISTS Groups (
    GroupId        INTEGER PRIMARY KEY AUTOINCREMENT,
    GroupName      TEXT NOT NULL,
    OwnerId        INTEGER NOT NULL,
    RequireApproval INTEGER NOT NULL DEFAULT 0,   -- 0 直接进 1 需群主审批
    CreatedTime    TEXT DEFAULT (datetime('now','localtime'))
);
-- 群号从 200001 起（与 QQ 号 10001 明显区分）
INSERT INTO sqlite_sequence(name, seq)
    SELECT 'Groups', 200000
    WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name='Groups');

CREATE TABLE IF NOT EXISTS GroupMembers (
    GroupId  INTEGER NOT NULL,
    UserId   INTEGER NOT NULL,
    Role     INTEGER NOT NULL DEFAULT 0,          -- 0 成员 1 群主
    JoinTime TEXT DEFAULT (datetime('now','localtime')),
    UNIQUE(GroupId, UserId)
);

CREATE TABLE IF NOT EXISTS GroupMessages (
    MsgId    INTEGER PRIMARY KEY AUTOINCREMENT,
    GroupId  INTEGER NOT NULL,
    SenderId INTEGER NOT NULL,
    TypeId   INTEGER NOT NULL,                    -- 1 文本 5 图片 6 文件
    Content  TEXT NOT NULL,
    FileId   INTEGER,
    SendTime TEXT DEFAULT (datetime('now','localtime'))
);

-- 入群请求（申请或邀请）
CREATE TABLE IF NOT EXISTS GroupRequests (
    ReqId       INTEGER PRIMARY KEY AUTOINCREMENT,
    GroupId     INTEGER NOT NULL,
    TargetId    INTEGER NOT NULL,                 -- 将加入的用户
    InviterId   INTEGER NOT NULL DEFAULT 0,       -- 0=自己申请，>0=邀请人
    NeedInviteeOk INTEGER NOT NULL DEFAULT 0,     -- 邀请场景需被邀请人同意
    NeedOwnerOk INTEGER NOT NULL DEFAULT 0,       -- 需群主审批
    InviteeOk   INTEGER NOT NULL DEFAULT 0,
    OwnerOk     INTEGER NOT NULL DEFAULT 0,
    Status      INTEGER NOT NULL DEFAULT 0,       -- 0 待处理 1 完成入群 2 拒绝
    ResultAck   INTEGER NOT NULL DEFAULT 0,
    CreatedTime TEXT DEFAULT (datetime('now','localtime')),
    HandledTime TEXT
);

CREATE INDEX IF NOT EXISTS IX_GroupMembers_User ON GroupMembers(UserId);
CREATE INDEX IF NOT EXISTS IX_GroupMembers_Group ON GroupMembers(GroupId);
CREATE INDEX IF NOT EXISTS IX_GroupMessages_GroupMsg ON GroupMessages(GroupId, MsgId);
CREATE INDEX IF NOT EXISTS IX_GroupReq_Target ON GroupRequests(TargetId, Status);
CREATE INDEX IF NOT EXISTS IX_GroupReq_Group ON GroupRequests(GroupId, Status);

CREATE INDEX IF NOT EXISTS IX_Messages_SenderReceiverMsg
    ON Messages(SenderId, ReceiverId, MsgId);
CREATE INDEX IF NOT EXISTS IX_Messages_ReceiverSenderMsg
    ON Messages(ReceiverId, SenderId, MsgId);
CREATE INDEX IF NOT EXISTS IX_FriendRequests_ReceiverStatus
    ON FriendRequests(ReceiverId, Status, RequestId);
CREATE INDEX IF NOT EXISTS IX_FriendRequests_SenderResult
    ON FriendRequests(SenderId, ResultAcknowledged, Status, RequestId);

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
 (1,'文本消息'),(2,'系统消息'),(3,'好友请求'),(4,'好友请求回执'),
 (5,'图片消息'),(6,'文件消息');
