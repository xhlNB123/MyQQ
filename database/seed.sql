-- =====================================================================
-- MyQQ 字典表初始化数据
-- =====================================================================

-- 星座
INSERT INTO Star (StarId, StarName) VALUES
 (1, N'白羊座'), (2, N'金牛座'), (3, N'双子座'), (4, N'巨蟹座'),
 (5, N'狮子座'), (6, N'处女座'), (7, N'天秤座'), (8, N'天蝎座'),
 (9, N'射手座'), (10, N'摩羯座'), (11, N'水瓶座'), (12, N'双鱼座');

-- 血型
INSERT INTO BloodType (BloodTypeId, BloodTypeName) VALUES
 (1, N'A'), (2, N'B'), (3, N'O'), (4, N'AB'), (5, N'未知');

-- 好友策略
INSERT INTO FriendshipPolicy (PolicyId, PolicyName) VALUES
 (1, N'允许任何人添加'), (2, N'需要验证信息'), (3, N'拒绝任何人添加');

-- 消息类型
INSERT INTO MessageType (TypeId, TypeName) VALUES
 (1, N'文本消息'), (2, N'系统消息'), (3, N'好友请求'), (4, N'好友请求回执');
