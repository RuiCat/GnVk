#include<GN.h>

struct sORDER  //定义一个结构 主要用来显示安全指针的数据创建和删除的过程
{
    int data;

	//当它创建时   打印创建信息到视图区并一起显示当前变量值     
	sORDER():data(-1){GN::RunRec.Print("sORDER create  data:%d\n", data);    } 

	sORDER(const sORDER& _s){ data = _s.data;
					  GN::RunRec.Print("sORDER clone   data:%d\n", data);    } 

	sORDER& operator = (const sORDER &_s){ data = _s.data; 
					  GN::RunRec.Print("sORDER clone = data:%d\n", data);  return (*this); }

	//当它销毁时   打印销毁信息到视图区并一起显示当前变量值
   ~sORDER(){         GN::RunRec.Print("sORDER destroy data:%d\n", data);    }
    
    //打印处变量值   
    void PrintData(){ GN::RunRec.Print("data = %d\n", data); }
};

P<sORDER>  GlobalP;   //定义一个 sORDER 的全局指针

extern "C"{
GN_CALL void AppInit(){     //▲初始化应用必须数据 程序开始准备好引擎系统后会调用它一次 

	//!当有信息打印到屏幕时会在左上角出现小!号浮动 鼠标点击可打开详细查看结果来分析安全指针指向空间的创建和删除
	GN::RunRec.Print("-------------------------- ( 1 )\n"); //打印步骤1信息栏 用于区分当前运行流程
	P<sORDER> LocalP;        //定义一个 sORDER 结构的局部临时安全指针
	LocalP.New(1);           //为局部临时指针分配创建1个数据空间
	LocalP->data = 100;      //可按普通指针一样引用成员变量
	
	GN::RunRec.Print("-------------------------- ( 2 )\n");
	GlobalP.New(3, *LocalP); //为全局安全指针分配3个数据空间 并使该3个数据都设置成和 LocalP所指向的数据相同
	unsigned int i = 0;
	for(; i < 3; ++i)GlobalP[i].PrintData(); //全局安全指针分配了3个数据空间的数组就可以像指针数组那样使用[i]来索引成员 打印内部值出来
    
	GN::RunRec.Print("-------------------------- ( 3 )\n");
	GlobalP.New(1);			 //全局安全指针重新分配一个数据空间 此时它原先所指向的数据如果没被其他安全指针共享使用会被自动删除
	GlobalP->data = 12;
    GlobalP->PrintData();	 //打印刚才新设置的值出来

	GN::RunRec.Print("-------------------------- ( 4 )\n");
    GlobalP = LocalP;        //使全局安全指针等于局部指针 这样全局指针和局部指针都指向一片相同的数据区 此时全局指针原先所指向的数据如果没被其他安全指针共享使用会被自动删除 而不会象普通指针那样造成内存泄露
	LocalP->data = 256;      //修改局部指针指向的值
    GlobalP->PrintData();    //打印全局安全指针指向的值出来看看是否也一起改变了

	GN::RunRec.Print("-------------------------- ( 5 )\n");
    LocalP.Delete();		 //显式删除局部安全指针所指向的数据 此时该指针已经失效了 相当于空指针 但考虑到它之前所指向的数据有被其它指针共享 所以并不会把那片数据区都删除掉 不象普通指针那样就直接删掉 所以它更安全
	if(!LocalP)GN::RunRec.Print("LocalP is empty!\n");  //确认该指针要是空的就输出提示
	GlobalP->PrintData();    //再打印全局安全指针指向的值出来看看! 注如果是使用普通指针这就崩了 会内存访问出轨 现在它所指向的数据没有因为局部指针删除而被一起删除 这可以有效地解决了在同一个类里的两个对象变量里有指针都指向了同一片数据 之后在该类的析构函数里设置自动对指针指向数据删除的问题!
	GlobalP.Delete();        //显式删除安全指针指向数据 今次由于已经没有其他指针共享了这片数据区 所以数据就真的是删除掉了

	GN::RunRec.Print("-------------------------- ( 6 )\n");
	sORDER  local_array[4];  //定以一个有4个数据空间的普通对象数组
	for(i=0; i<4; ++i)local_array[i].data = i; //为这4个内的变量赋值
	LocalP.New(local_array, 4);  //这样使用安全指针初始化数据比普通数组更方便 此时局部安全指针会分配4个数据空间 并把参数1数组的前4个结构的值都复制到安全指针所指向的数据区内
	GlobalP.Copy(LocalP);        //安全指针之间也可以这样来整片数据复制 和 GlobalP = LocalP 不同 现在它所指向的不是和LocalP指向同一片数据区 而是新复制出数据来 如果GlobalP之前有指向数据也不用担心忙记删除带来内存泄漏 因为它都会自动根据共享数来清除的
	
    GN::RunRec.Print("-------------------------- ( 7 )\n");
	local_array[0].data = 4, local_array[1].data = 5, local_array[2].data = 6, local_array[3].data = 7; 
    LocalP     [0].data = 8, LocalP     [1].data = 9, LocalP     [2].data =10, LocalP     [3].data =11; //都改变一下里面的数据值
	for(i=0; i<4;             ++i)local_array[i].PrintData(); 
	for(i=0; i<4;             ++i)LocalP     [i].PrintData();
	for(i=0; i<GlobalP.Num(); ++i)GlobalP    [i].PrintData(); //都显示它们内的值出来看看是否都一起跟着改变了? 是否注意到它门的引用操作都一样的吗!!

    GN::RunRec.Print("-------------------------- ( 8 )\n");
	P<sORDER> local2 = GlobalP.CopyOut(3, local_array[0]);    //以原 GlobalP 内的数据再扩充3个数据空间来复制出一个新的安全指针 并使新增的值等于 local_array[0]
    for(i=0; i<local2.Num(); ++i)local2[i].PrintData();		  //显示出新复制出的安全指针所指的全部数据值
	local2.Delete();										  //提前释放这个指针所指的数据空间 而不是等到函数结束时

	GN::RunRec.Print("-------------------------- ( 9 )\n");
	P< P< char > >  strings; //您也可嵌套安全指针
	strings.New(3);          //分配3个字符串指针
	strings[0].New("GN safe pointer sample", 8); //字符指针0新分配8个字符空间 并把参数1数组的前8个字符复制到它里面
	strings[0][7] = '\0';						 //使最后一个为结束符 不然直接打印显示的话会导致方位出轨
	strings[1].New("GN safe pointer sample", 0); //如果是字符串 第二个参数可以为0 这样就会刚好把参数1的内容全复制进安全指针里面去 包含结束符 使用起来比较方便!
    strings[2](    "GN safe pointer sample", 0); //扩号( )的作用 其实同等于New() 只是省略的写法而已
    
	for(i=0; i<strings.Num(); ++i)GN::RunRec.Print("%s\n",(char*)strings[i]); //输出strings指针里的所有字符串内容 (char*)strings[i] 这样可以把安全指针转换为普通指针 平时传给明确类型的参数您可不必多此一举 只是 Print 的参数为可变类型所以要明示
    
	P< uint8 > str_number = CVP(uint8)strings[0];  //安全指针之间的强制转换
    for(i=0; i<str_number.Num(); ++i)GN::RunRec.Print("%d,",(uint8*)str_number[i]); //显示代表字符编号的数值
	GN::RunRec.Print("\n");
	
	GN::RunRec.Print("-------------------------- (end)\n");
	GlobalP.Delete();  //全局指针必需要显式地删除数据 否侧它里面所指向的数据就只能等程序结束时才会自动释放 因为全局变量不会在函数处理完像局部变量那样自动被销毁
    GN::RunRec.Print("GlobalP deleted!!\n");
	
	//以下的这3个安全指针因为是局部指针 就算没手动调用删除 在该函数代码运行结束时也将会自动销毁掉 避免了一时大意忘记删除造成内存泄露
	//strings.Delete();    //手动要求删除局部安全字符串指针 strings 此时它里面所嵌套的全部安全指针里面的数据也会按没再被共享的就自动删除掉
	//local_array.Delete(); 
	//LocalP.Delete();
	
	/*-----------------------------------------------------------------------------\
	|	可见安全指针可以解决很多C++普通指针所带来的问题 而且操作简便               |
	|	使用时只是要记住是空的指针就要先分配好数据空间或使其指向引用共享其它安全   |
	|	指针 只有全局的安全指针才必需要手动删除数据                                |
	|	具体更多操作可参考 GN_Array.h 的 struct P 定义                             |
	\-----------------------------------------------------------------------------*/
}
GN_CALL void AppUnInit(){   //▲释放应用必须数据 程序运行结束前会调用它一次 
	
}
GN_CALL void AppDisplay2D(unsigned int nTime){ //▲显示2D画面 程序运行每帧都调用它 
	// 绘制背景矩型(没图片,  靠视口左边偏移0的坐标,   靠视口上边偏移0的坐标,        当前视口宽度,       当前视口高度,       暗蓝色
	GNg::ImageDraw(   0,      GNu::NearLx(),			 GNu::NearTy(0),		GNg::ViewCur->orthoW, GNg::ViewCur->orthoH, 0xff566888);
}
GN_CALL void AppDisplay3D(unsigned int nTime){} //▲显示3D画面 程序运行每帧都调用它 

GN_CALL void AppRun(unsigned int nTime){}       //▲运行程序后台的处理 程序运行每帧都调用它
}