#include "Interface.h"
#include "Evaluator.h"
#include "IControl.h"

enum ELayout
{
	kEditorWidth = GUI_WIDTH,
	kEditorHeight = GUI_HEIGHT,

	kExpression_X = 10,
	kExpression_Y = 10,
	kExpression_W = kEditorWidth - 20,
	kExpression_H = 20,

	kExprMsg_X = 10,
	kExprMsg_Y = kExpression_Y + 20,
	kExprMsg_W = kExpression_W,
	kExprMsg_H = 20,

	// the log window that shows the internal state of the expression
	kExprLog_X = 10,
	kExprLog_Y = kExprMsg_Y + 20,
	kExprLog_W = 140,
	kExprLog_H = 150,

	kExprLog_M = 5,   // margin
	kExprLog_TH = 12,  // text height
	kExprLog_TW = kExprLog_W - kExprLog_M * 2, // text width
};

// note: ICOLOR is ARGB
const IColor kBackgroundColor(255, 30, 30, 30);
const IColor kExprBackgroundColor(255, 100, 100, 100);
const IColor kTextColor(255, 180, 180, 180);
const IColor kGreenColor(255, 0, 210, 10);

IText  kExpressionTextStyle(11,
							&kGreenColor,
							"Courier",
							IText::kStyleNormal,
							IText::kAlignCenter,
							0, // orientation
							IText::kQualityDefault,
							&kExprBackgroundColor,
							&kGreenColor);

IText  kExprMsgTextStyle(11,
						&kTextColor,
						"Arial",
						IText::kStyleBold,
						IText::kAlignNear,
						0, // orientation
						IText::kQualityDefault);

IText  kExprLogTextStyle(11,
						&kGreenColor,
						"Courier",
						IText::kStyleNormal,
						IText::kAlignNear,
						0, // orientation
						IText::kQualityDefault);

IText kLabelTextStyle(12,
					&kTextColor,
					"Arial",
					IText::kStyleBold,
					IText::kAlignCenter,
					0, // orientation
					IText::kQualityDefault);

// originally cribbed from the IPlugEEL example
class ITextEdit : public IControl
{
public:
	ITextEdit(IPlugBase* pPlug, IRECT pR, int paramIdx, IText* pText, const char* str)
		: IControl(pPlug, pR)
		, mIdx(paramIdx)
	{
		mDisablePrompt = true;
		mText = *pText;
		mStr.Set(str);
		mTextEntryLength = kExpressionLengthMax;
	}

	~ITextEdit() {}

	bool Draw(IGraphics* pGraphics) override
	{
		return pGraphics->DrawIText(&mText, mStr.Get(), &mRECT);
	}

	void OnMouseDown(int x, int y, IMouseMod* pMod) override
	{
		mPlug->GetGUI()->CreateTextEntry(this, &mText, &mRECT, mStr.Get());
	}

	void TextFromTextEntry(const char* txt) override
	{
		mStr.Set(txt);
		SetDirty(false);
		mPlug->OnParamChange(mIdx);
	}

	const char * GetText() const
	{
		return mStr.Get();
	}

	int GetTextLength() const
	{
		return mStr.GetLength();
	}

protected:
	int        mIdx;
	WDL_String mStr;
};

class IIncrementControl : public IBitmapControl
{
public:
	IIncrementControl(IPlugBase* pPlug, int x, int y, int paramIdx, IBitmap* pBitmap, int direction)
		: IBitmapControl(pPlug, x, y, paramIdx, pBitmap)
		, mPressed(0)
	{
		IParam* param = GetParam();
		mInc = direction * 1.0 / (param->GetMax() - param->GetMin());
		mDblAsSingleClick = true;
	}

	bool Draw(IGraphics* pGraphics) override
	{
		return pGraphics->DrawBitmap(&mBitmap, &mRECT, mPressed + 1, &mBlend);
	}

	void OnMouseDown(int x, int y, IMouseMod* pMod) override
	{
		mPressed = 1;
		mValue = GetParam()->GetNormalized() + mInc;
		SetDirty();
	}

	void OnMouseUp(int x, int y, IMouseMod* pMod) override
	{
		mPressed = 0;
	}

private:
	double mInc;
	int   mPressed;
};

// helper for adding text controls to the log window
ITextControl* AttachLogText(IPlugBase* pPlug, IGraphics* pGraphics, int& y, const char* defaultText)
{
	const int xl = kExprLog_X + kExprLog_M;
	const int xr = xl + kExprLog_TW;

	ITextControl* control = new ITextControl(pPlug, IRECT(xl, y, xr, y + kExprLog_TH), &kExprLogTextStyle, defaultText);
	pGraphics->AttachControl(control);

	y += kExprLog_TH;

	return control;
}

Interface::Interface(Evaluator* plug, IGraphics* pGraphics)
: mPlug(plug)
{
	pGraphics->AttachPanelBackground(&kBackgroundColor);

	//--- Text input for the expression ------
	textEdit = new ITextEdit(mPlug, MakeIRect(kExpression), kExpression, &kExpressionTextStyle, "t*128");
	pGraphics->AttachControl(textEdit);

	ITextControl* textResult = new ITextControl(mPlug, MakeIRect(kExprMsg), &kExprMsgTextStyle);
	pGraphics->AttachControl(textResult);

	//-- "window" displaying internal state of the expression
	pGraphics->AttachControl(new IPanelControl(mPlug, MakeIRect(kExprLog), &COLOR_BLACK));
	{
		int y = kExprLog_Y + kExprLog_M;
		timeLabel = AttachLogText(mPlug, pGraphics, y, "t=0");
		millisLabel = AttachLogText(mPlug, pGraphics, y, "m=0");
		quartLabel = AttachLogText(mPlug, pGraphics, y, "q=0");
		rangeLabel = AttachLogText(mPlug, pGraphics, y, "r=0");
		noteLabel = AttachLogText(mPlug, pGraphics, y, "n=0");
		prevLabel = AttachLogText(mPlug, pGraphics, y, "p=0");
	}

	//---Volume--------------------
	{
		//---Volume Knob-------
		const int knobSize(35);
		const int knobLeft = kEditorWidth - knobSize - 10;
		const int knobTop = kEditorHeight - knobSize - 20;
		IRECT size(knobLeft, knobTop, knobLeft + knobSize, knobTop + knobSize);
		pGraphics->AttachControl(new IKnobLineControl(mPlug, size, kGain, &kGreenColor));

		//---Volume Label--------
		IRECT labelSize(size.L - 10, size.B - 5, size.R + 10, size.B + 10);
		pGraphics->AttachControl(new ITextControl(mPlug, labelSize, &kLabelTextStyle, "VOL"));
	}

	//---Bit Depth--------------
	{
		IBitmap numberBoxArrowUp = pGraphics->LoadIBitmap(NUMBERBOX_ARROW_UP_ID, NUMBERBOX_ARROW_UP_FN, 2);
		IBitmap numberBoxArrowDown = pGraphics->LoadIBitmap(NUMBERBOX_ARROW_DOWN_ID, NUMBERBOX_ARROW_DOWN_FN, 2);
		IBitmap numberBack = pGraphics->LoadIBitmap(NUMBERBOX_BACK_ID, NUMBERBOX_BACK_FN);

		//--Bit Depth Number Box Background
		IRECT backSize(0, 0, numberBack.W, numberBack.H);
		const int offX = kEditorWidth - backSize.W() - 10;
		const int offY = 50;
		backSize.L += offX;
		backSize.R += offX;
		backSize.T += offY;
		backSize.B += offY;

		pGraphics->AttachControl(new IBitmapControl(mPlug, backSize.L, backSize.T, &numberBack));

		//---Bit Depth Number Box Value--------
		const int textHH = 5;
		IRECT numberSize(backSize.L + 5, backSize.T + numberBack.H / 2 - textHH, backSize.L + 25, backSize.T + numberBack.H / 2 + textHH);
		bitDepthControl = new ICaptionControl(mPlug, numberSize, kBitDepth, &kExprLogTextStyle);
		pGraphics->AttachControl(bitDepthControl);

		//---Number Box Buttons
		int arrowX = backSize.R - numberBoxArrowUp.W;
		int arrowY = backSize.T + numberBack.H / 2 - numberBoxArrowUp.H / 2;
		pGraphics->AttachControl(new IIncrementControl(mPlug, arrowX, arrowY, kBitDepth, &numberBoxArrowUp, 1));
		pGraphics->AttachControl(new IIncrementControl(mPlug, arrowX, arrowY + numberBoxArrowUp.H / 2, kBitDepth, &numberBoxArrowDown, -1));

		//--Bit Depth Number Box Label
		IRECT boxLabelSize(backSize.L,
			backSize.B + 5,
			backSize.R,
			backSize.B + 25);
		pGraphics->AttachControl(new ITextControl(mPlug, boxLabelSize, &kLabelTextStyle, "BITS"));
	}
}


Interface::~Interface()
{
}

void Interface::SetDirty(int paramIdx, bool pushToPlug)
{
	switch (paramIdx)
	{
	case kBitDepth:
		if (bitDepthControl)
		{
			bitDepthControl->SetDirty(pushToPlug);
		}
		break;
	}
}

const char * Interface::GetProgramText() const
{
	return textEdit->GetText();
}

void Interface::SetProgramText(const char * programText)
{
	textEdit->TextFromTextEntry(programText);
}